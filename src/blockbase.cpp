#include <cmath>
#include <eosio/action.hpp>
#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/transaction.hpp>
#include <native.hpp>

#include <blockbase/blockbase.hpp>
#include <blockbasetoken/blockbasetoken.hpp>

#include <blockbase/blockproduce.hpp>
#include <blockbase/candidature.hpp>
#include <blockbase/consensus.hpp>
#include <blockbase/payments.hpp>
#include <blockbase/punishment.hpp>
#include <blockbase/service.hpp>

#pragma region State Actions

[[eosio::action]] void blockbase::startchain(eosio::name owner, std::string publicKey) {
    require_auth(owner);

    clientIndex _clients(_self, owner.value);
    stateIndex _states(_self, owner.value);

    auto client = _clients.find(owner.value);
    auto state = _states.find(owner.value);

    check(client == _clients.end(), "Client information already inserted, the chain has started.");
    check(state == _states.end(), "Chain status are already created.");
    check(IsPublicKeyValid(publicKey), "Public key is invalid, please insert a correct public key.");

    ChangeContractStateDAM({owner, true, false, false, false, false, false, false});

    _clients.emplace(owner, [&](auto &newClientI) {
        newClientI.key = owner;
        newClientI.public_key = publicKey;
    });
    eosio::print("Chain started. You can now insert your configurations. \n");
}

[[eosio::action]] void blockbase::configchain(eosio::name owner, blockbase::contractinfo infoJson, std::vector<eosio::name> reservedSeats) {
    require_auth(owner);

    stateIndex _states(_self, owner.value);
    producersIndex _producers(_self, owner.value);
    reservedseatIndex _reserverseats(_self, owner.value);
    auto state = _states.find(owner.value);
    auto numberOfProducersRequired = infoJson.number_of_validator_producers_required + infoJson.number_of_history_producers_required + infoJson.number_of_full_producers_required;
    check(infoJson.key.value == owner.value, "Key in configurations isn't the same account as the sidechain owner");
    check(state != _states.end() && state->has_chain_started != false && state->is_production_phase == false, "This sidechain hasnt't been created yet, please create it first.");
    check(IsConfigurationValid(infoJson), "The configurantion inserted is incorrect or not valid, please insert it again.");
    eosio::asset ownerStake = blockbasetoken::get_stake(BLOCKBASE_TOKEN, owner, owner);
    check(ownerStake.amount > MIN_REQUESTER_STAKE, "No stake inserted or the amount is not valid. Please insert your stake and configure the chain again. \n");
    check(reservedSeats.size() <= numberOfProducersRequired, "Number of reserved seats is superior to number of producers required");

    ChangeContractStateDAM({owner, true, true, false, false, false, false, false});
    UpdateContractInfoDAM(owner, infoJson);
    if (std::distance(_producers.begin(), _producers.end()) > 0) {
        RemoveBlockCountDAM(owner);
        RemoveIPsDAM(owner);
        RemoveProducersDAM(owner);
    }

    if (!reservedSeats.empty()) {
        for (auto seat : reservedSeats) {
            _reserverseats.emplace(owner, [&](auto &reservedSeatI) {
                reservedSeatI.key = seat;
            });
        }
    } else {
        auto iterator = _reserverseats.begin();
        while (iterator != _reserverseats.end())
            iterator = _reserverseats.erase(iterator);
    }

    eosio::print("Information inserted. \n");
}

[[eosio::action]] void blockbase::startcandtime(eosio::name owner) {
    require_auth(owner);
    eosio::print("Configuration time ended. \n");

    infoIndex _infos(_self, owner.value);
    stateIndex _states(_self, owner.value);
    auto info = _infos.find(owner.value);
    auto state = _states.find(owner.value);

    check(IsCandidaturePhase(owner), "The chain is not in the correct state, please check the current state of the chain. \n");
    check(info != _infos.end(), "No configuration inserted, please insert the configuration first. \n");

    SetEndDateDAM(owner, CANDIDATURE_TIME_ID);

    ChangeContractStateDAM({owner, true, false, true, false, false, false, state->is_production_phase});

    eosio::print("Start candidature time. \n");
}

[[eosio::action]] void blockbase::secrettime(eosio::name owner) {
    require_auth(owner);
    eosio::print("Candidature time ended. \n");

    candidatesIndex _candidates(_self, owner.value);
    producersIndex _producers(_self, owner.value);
    stateIndex _states(_self, owner.value);
    infoIndex _infos(_self, owner.value);
    auto state = _states.find(owner.value);
    auto info = _infos.find(owner.value);

    check(info != _infos.end(), "No configuration inserted, please insert the configuration first.");
    check(state != _states.end() && state->has_chain_started != false && state->is_candidature_phase != false, "The chain is not in the correct state, please check the current state of the chain.");
    check(eosio::current_block_time().to_time_point().sec_since_epoch() >= info->candidature_phase_end_date_in_seconds && info->candidature_phase_end_date_in_seconds != 0, "The candidature time didn't finnish yet, please check the contract information for more details.");

    auto numberOfProducersRequired = info->number_of_validator_producers_required + info->number_of_history_producers_required + info->number_of_full_producers_required;
    auto producersAndCandidatesInSidechainCount = std::distance(_producers.begin(), _producers.end()) + std::distance(_candidates.begin(), _candidates.end());
    if (producersAndCandidatesInSidechainCount < numberOfProducersRequired) {
        eosio::print("Starting candidature time again... \n");
        SetEndDateDAM(owner, CANDIDATURE_TIME_ID);
        ChangeContractStateDAM({owner, true, false, true, false, false, false, state->is_production_phase});
        return;
    }

    ChangeContractStateDAM({owner, true, false, false, true, false, false, state->is_production_phase});

    SetEndDateDAM(owner, SECRET_TIME_ID);

    eosio::print("Start Secret send time. \n");
}

[[eosio::action]] void blockbase::startsendtime(eosio::name owner) {
    require_auth(owner);
    eosio::print("Secret send time ended. \n");

    producersIndex _producers(_self, owner.value);
    stateIndex _states(_self, owner.value);
    infoIndex _infos(_self, owner.value);

    auto state = _states.find(owner.value);
    auto info = _infos.find(owner.value);

    check(info != _infos.end(), "No configuration inserted, please insert the configuration first. \n");
    check(state != _states.end() && state->has_chain_started != false && state->is_secret_sending_phase != false, "The chain is not in the correct state, please check the current state of the chain. \n");
    check(eosio::current_block_time().to_time_point().sec_since_epoch() >= info->secret_sending_phase_end_date_in_seconds && info->secret_sending_phase_end_date_in_seconds != 0, "The secrect time didn't finnish yet, please check the contract information for more details.");

    AddCandidatesWithReservedSeat(owner);

    auto producersInSidechainCount = std::distance(_producers.begin(), _producers.end());
    auto numberOfProducersRequired = info->number_of_validator_producers_required + info->number_of_history_producers_required + info->number_of_full_producers_required;

    std::vector<struct blockbase::candidates> selectedCandidateList = RunCandidatesSelection(owner);
    if (producersInSidechainCount + selectedCandidateList.size() < numberOfProducersRequired) {
        if (producersInSidechainCount < ceil((numberOfProducersRequired)*MIN_PRODUCERS_IN_CHAIN_THRESHOLD)) {
            ChangeContractStateDAM({owner, true, false, false, false, false, false, false});
            RemoveBlockCountDAM(owner);
            RemoveIPsDAM(owner);
            RemoveProducersDAM(owner);
            eosio::print("Not enough candidates, configure the chain again. \n");
        } else {
            ChangeContractStateDAM({owner, true, false, true, false, false, false, state->is_production_phase});
            eosio::print("Starting candidature time again... \n");
            SetEndDateDAM(owner, CANDIDATURE_TIME_ID);
            return;
        }
    } else {
        for (auto candidate : selectedCandidateList) {
            AddProducerDAM(owner, candidate);
            AddPublicKeyDAM(owner, candidate.key, candidate.public_key);
            RemoveCandidateDAM(owner, candidate.key);
        }
        eosio::print("Producers sucessfully inserted. \n");
        SetEndDateDAM(owner, SEND_TIME_ID);

        ChangeContractStateDAM({owner, true, false, false, false, true, false, state->is_production_phase});

        eosio::print("Start send time. \n");
    }
}

[[eosio::action]] void blockbase::startrectime(eosio::name owner) {
    require_auth(owner);
    eosio::print("Send time ended. \n");
    producersIndex _producers(_self, owner.value);
    infoIndex _infos(_self, owner.value);
    stateIndex _states(_self, owner.value);

    auto state = _states.find(owner.value);
    auto info = _infos.find(owner.value);

    check(state != _states.end() && state->has_chain_started != false && state->is_ip_sending_phase != false, "The chain is not in the correct state, please check the current state of the chain.");
    check(info != _infos.end(), "No configuration inserted, please insert the configuration first.");
    check(eosio::current_block_time().to_time_point().sec_since_epoch() >= info->ip_sending_phase_end_date_in_seconds && info->ip_sending_phase_end_date_in_seconds != 0, "The ip send time didn't finnish yet, please check the contract information for more details.");

    std::vector<struct blockbase::producers> producersWhoFailedToSendIPsList = GetProducersWhoFailedToSendIPs(owner);
    if (producersWhoFailedToSendIPsList.size() > 0) {
        RemoveIPsDAM(owner, producersWhoFailedToSendIPsList);
        RemoveProducersDAM(owner, producersWhoFailedToSendIPsList);
        auto numberOfProducersRequired = info->number_of_validator_producers_required + info->number_of_history_producers_required + info->number_of_full_producers_required;

        if (std::distance(_producers.begin(), _producers.end()) < ceil(numberOfProducersRequired * MIN_PRODUCERS_IN_CHAIN_THRESHOLD)) {
            ChangeContractStateDAM({owner, true, false, false, false, false, false, false});
            eosio::print("Configure chain again. \n");
            return;
        } else {
            ChangeContractStateDAM({owner, true, false, true, false, false, false, state->is_production_phase});

            eosio::print("Candidature time started again. \n");
            SetEndDateDAM(owner, CANDIDATURE_TIME_ID);
            return;
        }
    }

    ChangeContractStateDAM({owner, true, false, false, false, false, true, state->is_production_phase});
    ResetBlockCountDAM(owner);
    SetEndDateDAM(owner, RECEIVE_TIME_ID);
    eosio::print("Start receive time. \n");
}

[[eosio::action]] void blockbase::startprodtime(eosio::name owner) {
    require_auth(owner);
    eosio::print("End receive time. \n");

    infoIndex _infos(_self, owner.value);
    stateIndex _states(_self, owner.value);
    auto state = _states.find(owner.value);
    auto info = _infos.find(owner.value);
    check(state != _states.end() && state->has_chain_started != false && state->is_ip_retrieving_phase != false, "The chain is not in the correct state, please check the current state of the chain. \n");
    check(info != _infos.end(), "No configuration inserted, please insert the configuration first. \n");
    check(eosio::current_block_time().to_time_point().sec_since_epoch() >= info->ip_retrieval_phase_end_date_in_seconds && info->ip_retrieval_phase_end_date_in_seconds != 0, "The ip receive time didn't finnish yet, please check the contract information for more details.");

    ChangeContractStateDAM({owner, true, false, false, false, false, false, true});

    if (state->is_production_phase)
        return;

    eosio::print("And let the production begin! \n");
    eosio::print("Inserting current producer. \n");
    currentprodIndex _currentprods(_self, owner.value);
    std::vector<blockbase::producers> readyProducersList = GetReadyProducers(owner);
    if (std::distance(_currentprods.begin(), _currentprods.end()) == 0 && readyProducersList.size() > 0) {
        struct blockbase::producers nextproducer = GetNextProducer(owner);
        if (nextproducer.is_ready_to_produce)
            UpdateCurrentProducerDAM(owner, nextproducer.key);
    }
}

#pragma endregion
#pragma region User Actions

[[eosio::action]] void blockbase::addcandidate(eosio::name owner, eosio::name candidate, uint64_t &workDurationInSeconds, std::string &publicKey, checksum256 secretHash, uint8_t producerType) {
    require_auth(candidate);

    stateIndex _states(_self, owner.value);
    blacklistIndex _blacklists(_self, owner.value);
    infoIndex _infos(_self, owner.value);

    auto state = _states.find(owner.value);
    auto blackListedAccount = _blacklists.find(candidate.value);
    auto info = _infos.find(owner.value);

    check(blackListedAccount == _blacklists.end(), "This account is blacklisted and can't enter this sidechain. \n");
    check(state != _states.end() && state->has_chain_started != false && state->is_candidature_phase != false, "The chain is not in the correct state, please check the current state of the chain. \n");
    check(IsCandidateValid(owner, candidate, workDurationInSeconds), "Candidature is invalid, please check the inserted values. \n");
    check(IsPublicKeyValid(publicKey), "Incorrect format in public key, try inserting again. \n");
    eosio::asset candidateStake = blockbasetoken::get_stake(BLOCKBASE_TOKEN, owner, candidate);
    check(candidateStake.amount > 0, "No stake inserted in the sidechain. Please insert a stake first.\n");
    check(candidateStake.amount > info->min_candidature_stake, "Stake inserted is not enough. Please insert more stake to be able to apply.\n");
    check(producerType == 1 || producerType == 2 || producerType == 3, "Incorrect producer type. Pleace choose a correct producer type");
    AddCandidateDAM(owner, candidate, workDurationInSeconds, publicKey, secretHash, producerType);
    eosio::print("Candidate added. \n");
}

[[eosio::action]] void blockbase::addencryptip(eosio::name owner, eosio::name producer, std::vector<std::string> encryptedIps) {
    require_auth(producer);

    stateIndex _states(_self, owner.value);

    auto state = _states.find(owner.value);
    check(state != _states.end() && state->has_chain_started != false && state->is_ip_sending_phase != false, "The chain is not in the correct state, please check the current state of the chain. \n");

    ipsIndex _ips(_self, owner.value);
    auto ip = _ips.find(producer.value);

    _ips.modify(ip, producer, [&](auto &ipaddress) {
        ipaddress.encrypted_ips.clear();
        for (auto iplist : encryptedIps)
            ipaddress.encrypted_ips.push_back(iplist);
    });

    eosio::print("Information successfully inserted. \n");
}

[[eosio::action]] void blockbase::addsecret(eosio::name owner, eosio::name producer, checksum256 secret) {
    require_auth(producer);

    stateIndex _states(_self, owner.value);
    infoIndex _infos(_self, owner.value);
    candidatesIndex _candidates(_self, owner.value);

    auto info = _infos.find(owner.value);
    auto state = _states.find(owner.value);
    auto candidate = _candidates.find(producer.value);

    check(state != _states.end() && state->has_chain_started != false && state->is_secret_sending_phase == true, "The chain is not in the correct state, please check the current state of the chain. \n");
    check(info != _infos.end(), "The chain doesn't have any configurations inserted. Please insert configurations to begin production. \n");
    check(IsSecretValid(owner, producer, secret), "Secret is invalid, please insert a valid secret");
    check(candidate != _candidates.end(), "Your account was not selected for the producing pool. \n");

    _candidates.modify(candidate, producer, [&](auto &candidateI) {
        candidateI.secret = secret;
    });
    eosio::print("Secret inserted. \n");
}

[[eosio::action]] void blockbase::addblock(eosio::name owner, eosio::name producer, blockbase::blockheaders block) {
    require_auth(producer);

    stateIndex _states(_self, owner.value);
    producersIndex _producers(_self, owner.value);

    auto state = _states.find(owner.value);
    auto producerInSidechain = _producers.find(producer.value);
    check(state != _states.end() && state->has_chain_started != false && state->is_production_phase != false, "The chain is not in the correct state, please check the current state of the chain. \n");
    check(producerInSidechain != _producers.end(), "Producer not in pool. \n");
    check(IsProducerTurn(owner, producer), "It's not this producer turn to produce a block. \n");
    check(IsBlockValid(owner, block), "Invalid Block. \n");
    check(HasBlockBeenProduced(owner, producer), "You already produced in this time slot, wait for your turn. \n");
    AddBlockDAM(owner, producer, block);
    eosio::print("Block submited with success. \n");
}

[[eosio::action]] void blockbase::rcandidate(eosio::name owner, eosio::name producer) {
    require_auth(producer);

    stateIndex _states(_self, owner.value);
    candidatesIndex _candidates(_self, owner.value);

    auto state = _states.find(owner.value);
    auto candidateInSidechainToRemove = _candidates.find(producer.value);
    check(state != _states.end() && state->has_chain_started != false, "The chain is not in the correct state, please check the current state of the chain. \n");
    check(candidateInSidechainToRemove != _candidates.end(), "Candidate can't be removed. Candidate doesn't exist in the candidate list. \n");

    RemoveCandidateDAM(owner, candidateInSidechainToRemove->key);
}

[[eosio::action]] void blockbase::resetreward(eosio::name owner, eosio::name producer) {
    require_auth(BLOCKBASE_TOKEN);
    rewardsIndex _rewards(_self, producer.value);
    producersIndex _producers(_self, owner.value);

    auto rewardForProducer = _rewards.find(owner.value);
    auto producerI = _producers.find(producer.value);
    if (rewardForProducer != _rewards.end() && rewardForProducer->reward > 0) {
        _rewards.modify(rewardForProducer, same_payer, [&](auto &rewardI) {
            rewardI.reward = 0;
        });
    }
    if (producerI == _producers.end())
        _rewards.erase(rewardForProducer);
}

[[eosio::action]] void blockbase::removeblisted(eosio::name owner, eosio::name producer) {
    require_auth(owner);
    producersIndex _producers(_self, owner.value);
    candidatesIndex _candidates(_self, owner.value);
    blacklistIndex _blacklists(_self, owner.value);
    auto blackListedProducer = _blacklists.find(producer.value);
    check(_blacklists.find(producer.value) != _blacklists.end(), "This user is not in the blacklist of this smart contract. \n");
    check(_producers.find(producer.value) == _producers.end() && _candidates.find(producer.value) == _candidates.end(), "This producer is currently in producer pool \n");
    _blacklists.erase(blackListedProducer);
}

[[eosio::action]] void blockbase::iamready(eosio::name owner, eosio::name producer) {
    require_auth(producer);
    producersIndex _producers(_self, owner.value);
    auto producerInSidechain = _producers.find(producer.value);
    check(producerInSidechain != _producers.end(), "This producer doesn't exist. \n");
    check(producerInSidechain->is_ready_to_produce != true, "This producer is already ready. \n");
    _producers.modify(producerInSidechain, producer, [&](auto &producerI) {
        producerI.is_ready_to_produce = true;
    });
    eosio::print("Producer ", producer, "is ready and will start producting. \n");
}

[[eosio::action]] void blockbase::extendwrktime(eosio::name owner, eosio::name producer, uint64_t &worktimeToAddInSeconds) {
    require_auth(producer);
    producersIndex _producers(_self, owner.value);
    auto producerI = _producers.find(producer.value);
    check(producerI != _producers.end(), "Producer doesn't exist. \n");
    _producers.modify(producerI, producer, [&](auto &producerIT) {
        producerIT.work_duration_in_seconds = producerIT.work_duration_in_seconds + worktimeToAddInSeconds;
    });
}

[[eosio::action]] void blockbase::stopproducing(eosio::name owner, eosio::name producer) {
    require_auth(producer);
    producersIndex _producers(_self, owner.value);
    stateIndex _states(_self, owner.value);
    blockheadersIndex _blockheaders(_self, owner.value);

    auto producerI = _producers.find(producer.value);
    auto state = _states.find(owner.value);
    std::vector<struct blockbase::blockheaders> lastblock = blockbase::GetLatestBlock(owner);

    check(producerI != _producers.end(), "Producer doesn't exist. \n");
    check((state != _states.end() &&
           state->is_production_phase == false &&
           state->is_secret_sending_phase == false &&
           state->is_ip_sending_phase == false &&
           state->is_ip_retrieving_phase == false) ||
              lastblock.back().timestamp + 259200 < eosio::current_block_time().to_time_point().sec_since_epoch(),
          "The chain is still in production so producer can't leave");

    auto producerToRemove = _producers.find(producer.value);
    _producers.erase(producerToRemove);
}

[[eosio::action]] void blockbase::changecprod(eosio::name owner) {
    require_auth(owner);

    eosio::print("Updating current producer... \n");

    infoIndex _infos(_self, owner.value);
    stateIndex _states(_self, owner.value);
    clientIndex _clients(_self, owner.value);
    currentprodIndex _currentprods(_self, owner.value);
    blockscountIndex _blockscount(_self, owner.value);

    auto info = _infos.find(owner.value);
    auto state = _states.find(owner.value);
    auto client = _clients.find(owner.value);

    check(state != _states.end() && state->has_chain_started != false && state->is_production_phase != false, "The chain is not in the correct state, please check the current state of the chain. \n");
    check(info != _infos.end(), "The chain doesn't have any configurations inserted. Please insert configurations to begin production. \n");
    check(client != _clients.end(), "No client information in the chain. Please insert the needed information. \n");

    std::vector<blockbase::producers> readyProducerslist = GetReadyProducers(owner);
    if (readyProducerslist.size() > 0) {
        auto currentProducer = _currentprods.begin();

        UpdateBlockCount(owner, currentProducer->producer);
        auto blockCountForComputation = 0;
        for (auto count : _blockscount) {
            blockCountForComputation += count.num_blocks_produced;
            blockCountForComputation += count.num_blocks_failed;
        }

        if (blockCountForComputation >= info->num_blocks_between_settlements) {
            eosio::print("Computation time... \n");
            RunSettlement(owner);
        }
        readyProducerslist = GetReadyProducers(owner);
        if (readyProducerslist.size() > 0) {
            struct blockbase::producers nextproducer = GetNextProducer(owner);
            if ((currentProducer->production_start_date_in_seconds + (info->block_time_in_seconds) / 2) <= eosio::current_block_time().to_time_point().sec_since_epoch()) {
                UpdateCurrentProducerDAM(owner, nextproducer.key);
                eosio::print("Current producer updated. \n");
            } else
                eosio::print("Same producer producing blocks. \n");
        }
    }
    ReOpenCandidaturePhaseIfRequired(owner);
}

[[eosio::action]] void blockbase::verifyblock(eosio::name owner, eosio::name producer, std::string blockHash) {
    require_auth(owner);

    blockheadersIndex _blockheaders(_self, owner.value);
    infoIndex _infos(_self, owner.value);

    auto info = _infos.find(owner.value);
    std::vector<struct blockbase::blockheaders> lastblock = blockbase::GetLatestBlock(owner);
    auto blockToValidate = _blockheaders.find((--_blockheaders.end())->sequence_number);

    if (lastblock.size() > 0)
        check(lastblock.back().timestamp + (info->block_time_in_seconds) < eosio::current_block_time().to_time_point().sec_since_epoch(), "Time to verify block already passed");

    if (std::distance(_blockheaders.begin(), _blockheaders.end()) > 0) {
        if (blockToValidate->block_hash == blockHash && blockToValidate->is_verified == false && blockToValidate->is_latest_block == false) {
            _blockheaders.modify(blockToValidate, _self, [&](auto &newBlockI) {
                newBlockI.is_verified = true;
                newBlockI.is_latest_block = true;
            });
            if (lastblock.size() > 0) {
                auto blockToModify = _blockheaders.find(lastblock.back().sequence_number);
                _blockheaders.modify(blockToModify, _self, [&](auto &newBlockI) {
                    newBlockI.is_latest_block = false;
                });
            }
            eosio::print("Block validated. \n");
            return;
        }
    }
}

[[eosio::action]] void blockbase::blacklistprod(eosio::name owner) {
    require_auth(owner);
    producersIndex _producers(_self, owner.value);
    blacklistIndex _blacklists(_self, owner.value);
    currentprodIndex _currentprods(_self, owner.value);
    for (auto producer : _producers) {

        auto blackListedProducer = _blacklists.find(producer.key.value);
        if (producer.warning_type == WARNING_TYPE_PUNISH && blackListedProducer == _blacklists.end()) {
            _blacklists.emplace(owner, [&](auto &blackListedProducerI) {
                blackListedProducerI.key = producer.key;
            });
        }
    }
    RemoveBadProducers(owner);
    ReOpenCandidaturePhaseIfRequired(owner);
    if (std::distance(_producers.begin(), _producers.end()) != 0 && std::distance(_currentprods.begin(), _currentprods.end()) == 0) {
        UpdateCurrentProducerDAM(owner, (_producers.begin())->key);
    }
}

[[eosio::action]] void blockbase::reqhistval(eosio::name owner, eosio::name producer, std::string blockHash) {
    require_auth(owner);
    histvalIndex _histval(_self, owner.value);
    _histval.emplace(owner, [&](auto &historyValidationI) {
        historyValidationI.key = producer;
        historyValidationI.block_hash = blockHash;
    });
}

[[eosio::action]] void blockbase::addblckbyte(eosio::name owner, eosio::name producer, std::string byteInHex) {
    require_auth(producer);
    histvalIndex _histval(_self, owner.value);
    auto itr = _histval.begin();
    check(itr != _histval.end(), "No validation request inserted");
    check(itr->key.value == producer.value, "Not requested producer");

    _histval.modify(itr, producer, [&](auto &historyValidationI) {
        historyValidationI.block_byte_in_hex = byteInHex;
    });
}

[[eosio::action]] void blockbase::histvalidate(eosio::name owner, eosio::name producer) {
    require_auth(owner);
    histvalIndex _histval(_self, owner.value);
    producersIndex _producers(_self, owner.value);
    auto itr = _histval.begin();
    while (itr != _histval.end()) {
        if(itr -> key.value == producer.value) {
            auto producerInTable = _producers.find(itr->key.value);
            if (producerInTable -> warning_type == WARNING_TYPE_FLAGGED) UpdateWarningDAM(owner, producer, WARNING_TYPE_CLEAR);
            _histval.erase(itr);
        }
    }
}

[[eosio::action]] void blockbase::endservice(eosio::name owner) {
    require_auth(owner);
    eosio::print("Ending Service. \n");
    infoIndex _infos(_self, owner.value);
    stateIndex _states(_self, owner.value);
    clientIndex _clients(_self, owner.value);
    currentprodIndex _currentprods(_self, owner.value);
    candidatesIndex _candidates(_self, owner.value);
    blacklistIndex _blacklists(_self, owner.value);
    rewardsIndex _rewards(_self, owner.value);
    blockheadersIndex _blockheaders(_self, owner.value);

    RemoveProducersDAM(owner);
    RemoveIPsDAM(owner);
    RemoveBlockCountDAM(owner);

    auto itr = _infos.begin();
    while (itr != _infos.end())
        itr = _infos.erase(itr);

    auto itr1 = _states.begin();
    while (itr1 != _states.end())
        itr1 = _states.erase(itr1);

    auto itr2 = _clients.begin();
    while (itr2 != _clients.end())
        itr2 = _clients.erase(itr2);

    auto itr3 = _rewards.begin();
    while (itr3 != _rewards.end())
        itr3 = _rewards.erase(itr3);

    auto itr4 = _candidates.begin();
    while (itr4 != _candidates.end())
        itr4 = _candidates.erase(itr4);

    auto itr5 = _blacklists.begin();
    while (itr5 != _blacklists.end())
        itr5 = _blacklists.erase(itr5);

    auto itr6 = _currentprods.begin();
    while (itr6 != _currentprods.end())
        itr6 = _currentprods.erase(itr6);

    auto itr7 = _blockheaders.begin();
    while (itr7 != _blockheaders.end())
        itr7 = _blockheaders.erase(itr7);

    eosio::print("Service Ended. \n");
}

#pragma endregion