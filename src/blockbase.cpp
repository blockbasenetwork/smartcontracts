#include <cmath>
#include <eosio/action.hpp>
#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/transaction.hpp>
#include <native.hpp>
#include <eosio/binary_extension.hpp>

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
    check(IsPublicKeyValid(owner, publicKey), "Public key is invalid, please insert a correct public key.");

    ChangeContractStateDAM({owner, true, false, false, false, false, false, false});

    _clients.emplace(owner, [&](auto &newClientI) {
        newClientI.key = owner;
        newClientI.public_key = publicKey;
        newClientI.sidechain_creation_timestamp.emplace(
            eosio::current_block_time().to_time_point().sec_since_epoch()
        );
    });
    eosio::print("Chain started. You can now insert your configurations. \n");
}

[[eosio::action]] void blockbase::configchain(eosio::name owner, blockbase::contractinfo infoJson, std::vector<blockbase::reservedseat> reservedSeats, uint32_t softwareVersion, eosio::binary_extension<blockbase::blockheaders>& startingBlock) {
    require_auth(owner);

    stateIndex _states(_self, owner.value);
    producersIndex _producers(_self, owner.value);
    reservedseatIndex _reservedseats(_self, owner.value);

    auto existReservedSeatsCount = std::distance(_reservedseats.begin(), _reservedseats.end());
    auto newReservedSeatsCount = std::distance(reservedSeats.begin(), reservedSeats.end());
    auto reservedSeatsCount = existReservedSeatsCount + newReservedSeatsCount;
    auto state = _states.find(owner.value);

    check(infoJson.key.value == owner.value, "Account isn't the same account as the sidechain owner.");

    check(state != _states.end() && state->has_chain_started == true, "This sidechain hasnt't been created yet, please create it first.");
    check(state->is_production_phase == false && state->is_candidature_phase == false && state->is_secret_sending_phase == false && state->is_ip_sending_phase == false && state->is_ip_retrieving_phase == false, "The sidechain is already in production.");
    check(IsConfigurationValid(infoJson, reservedSeatsCount), "The configuration inserted is incorrect or not valid, please insert it again.");
    check(softwareVersion >= 1, "The version inserted is not valid"); // 1 represents the lowest software version 0.0.1
    eosio::asset ownerStake = blockbasetoken::get_stake(BLOCKBASE_TOKEN, owner, owner);
    check(ownerStake.amount > MIN_REQUESTER_STAKE, "No stake inserted or the amount is not valid. Please insert your stake and configure the chain again.");
    //check(reservedSeats.size() <= numberOfProducersRequired, "Number of reserved seats is bigger than the number of producers requested");

    ChangeContractStateDAM({owner, true, true, false, false, false, false, false});
    SoftwareVersionDAM(owner, softwareVersion);
    
    //TODO rpinto - why does it do a cleaning here? And why to these tables only?
    //reserved seats here instead if else.
    if (std::distance(_producers.begin(), _producers.end()) > 0) {
        RemoveBlockCountDAM(owner);
        RemoveIPsDAM(owner);
        RemoveProducersDAM(owner);
        auto iterator = _reservedseats.begin();
        while (iterator != _reservedseats.end())
            iterator = _reservedseats.erase(iterator);
    }

    if (!reservedSeats.empty()) {
        for (auto seat : reservedSeats) {
            auto reservedSeat = _reservedseats.find(seat.key.value);
            if(reservedSeat == _reservedseats.end() && is_account(seat.key)) { 
                _reservedseats.emplace(owner, [&](auto &reservedSeatI) {
                    reservedSeatI.key = seat.key;
                    reservedSeatI.producer_type = seat.producer_type;
                });
            }
        }
    }

    UpdateContractInfoDAM(owner, infoJson);

    if (startingBlock) {
        blockheadersIndex _blockheaders(_self, owner.value);
        auto block = startingBlock.value();

        _blockheaders.emplace(owner, [&](auto &newBlockI) {
            newBlockI.producer = block.producer;
            newBlockI.block_hash = block.block_hash;
            newBlockI.previous_block_hash = block.previous_block_hash;
            newBlockI.last_trx_sequence_number = block.last_trx_sequence_number;
            newBlockI.sequence_number = block.sequence_number;
            newBlockI.timestamp = block.timestamp;
            newBlockI.transactions_count = block.transactions_count;
            newBlockI.producer_signature = block.producer_signature;
            newBlockI.merkletree_root_hash = block.merkletree_root_hash;
            newBlockI.is_verified = true;
            newBlockI.is_latest_block = true;
            newBlockI.block_size_in_bytes = block.block_size_in_bytes;
        });
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

    check(IsCandidaturePhase(owner), "The chain is not in configuration phase, please check the current state of the chain.");
    check(info != _infos.end(), "No configuration inserted, please insert the configuration first.");

    SetEndDateDAM(owner, CANDIDATURE_TIME_ID);

    ChangeContractStateDAM({owner, true, false, true, false, false, false, state->is_production_phase});

    eosio::print("Started candidature time. \n");
}

[[eosio::action]] void blockbase::secrettime(eosio::name owner) {
    require_auth(owner);
    eosio::print("Candidature time ended. \n");

    candidatesIndex _candidates(_self, owner.value);
    producersIndex _producers(_self, owner.value);
    stateIndex _states(_self, owner.value);
    infoIndex _infos(_self, owner.value);
    reservedseatIndex _reservedseats(_self, owner.value);
    auto state = _states.find(owner.value);
    auto info = _infos.find(owner.value);
    auto reservedSeatsCount = std::distance(_reservedseats.begin(), _reservedseats.end());

    check(info != _infos.end(), "No configuration inserted, please insert the configuration first.");

    //TODO REFACTOR- why isn't this inside a method like IsSecretPhase()?
    check(state != _states.end() && state->has_chain_started == true && state->is_candidature_phase == true, "The chain is not in candidature phase, please check the current state of the chain.");
    
    check(eosio::current_block_time().to_time_point().sec_since_epoch() >= info->candidature_phase_end_date_in_seconds && info->candidature_phase_end_date_in_seconds != 0, "The candidature phase hasn't finished yet, please check the contract information for more details.");

    auto numberOfProducersRequired = info->number_of_validator_producers_required + info->number_of_history_producers_required + info->number_of_full_producers_required + reservedSeatsCount;
    auto numberOfCandidates = std::distance(_candidates.begin(), _candidates.end());
    auto numberOfProducersInChain = std::distance(_producers.begin(), _producers.end());
    
    auto producersAndCandidatesInSidechainCount = numberOfCandidates + numberOfProducersInChain;
    
    if (numberOfCandidates == 0 || !AreThereEmptySlotsForCandidateTypes(owner) || producersAndCandidatesInSidechainCount < ceil(numberOfProducersRequired * MIN_PRODUCERS_TO_PRODUCE_THRESHOLD)) {
        eosio::print("Starting candidature phase again... \n");
        SetEndDateDAM(owner, CANDIDATURE_TIME_ID);
        ChangeContractStateDAM({owner, true, false, true, false, false, false, state->is_production_phase});
    }
    else
    {
        ChangeContractStateDAM({owner, true, false, false, true, false, false, state->is_production_phase});
        SetEndDateDAM(owner, SECRET_TIME_ID);
        eosio::print("Start Secret send time. \n");
    }
}

[[eosio::action]] void blockbase::startsendtime(eosio::name owner) {
    require_auth(owner);
    eosio::print("Secret send time ended. \n");

    producersIndex _producers(_self, owner.value);
    stateIndex _states(_self, owner.value);
    infoIndex _infos(_self, owner.value);
    reservedseatIndex _reservedseats(_self, owner.value);

    auto state = _states.find(owner.value);
    auto info = _infos.find(owner.value);

    check(info != _infos.end(), "No configuration inserted, please insert the configuration first. \n");

    check(state != _states.end() && state->has_chain_started == true && state->is_secret_sending_phase == true, "The chain is not in the secret send phase, please check the current state of the chain.");
    check(eosio::current_block_time().to_time_point().sec_since_epoch() >= info->secret_sending_phase_end_date_in_seconds && info->secret_sending_phase_end_date_in_seconds != 0, "The secrect phase hasn't finished yet, please check the contract information for more details.");

    AddCandidatesWithReservedSeat(owner);
    std::vector<struct blockbase::candidates> candidatesToClear = GetCandidatesToClear(owner);
    RemoveCandidatesDAM(owner, candidatesToClear);

    auto producersInSidechainCount = std::distance(_producers.begin(), _producers.end());
    auto reservedSeatsCount = std::distance(_reservedseats.begin(), _reservedseats.end());
    auto numberOfProducersRequired = info->number_of_validator_producers_required + info->number_of_history_producers_required + info->number_of_full_producers_required + reservedSeatsCount;

    std::vector<struct blockbase::candidates> selectedCandidateList = RunCandidatesSelection(owner);

    if (producersInSidechainCount + selectedCandidateList.size() < ceil(numberOfProducersRequired * MIN_PRODUCERS_TO_PRODUCE_THRESHOLD)) {
        if (producersInSidechainCount + selectedCandidateList.size()< ceil((numberOfProducersRequired) * MIN_PRODUCERS_IN_CHAIN_THRESHOLD)) {
            ChangeContractStateDAM({owner, true, false, true, false, false, false, false});
            SetEndDateDAM(owner, CANDIDATURE_TIME_ID);
            RemoveBlockCountDAM(owner);
            eosio::print("Not enough candidates, starting candidature again \n");
        } else {
            ChangeContractStateDAM({owner, true, false, true, false, false, false, state->is_production_phase});
            SetEndDateDAM(owner, CANDIDATURE_TIME_ID);
            eosio::print("Starting candidature time again... \n");
        }
    }  else {
        for (auto candidate : selectedCandidateList) {
            AddProducerDAM(owner, candidate);
            UpdateWarningTimeInNewProducer(owner, candidate.key);
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
    reservedseatIndex _reservedseats(_self, owner.value);

    auto state = _states.find(owner.value);
    auto info = _infos.find(owner.value);

    check(state != _states.end() && state->has_chain_started == true && state->is_ip_sending_phase == true, "The chain is not in the IP sending phase, please check the current state of the chain.");
    check(info != _infos.end(), "No configuration inserted, please insert the configuration first.");
    check(eosio::current_block_time().to_time_point().sec_since_epoch() >= info->ip_sending_phase_end_date_in_seconds && info->ip_sending_phase_end_date_in_seconds != 0, "The IP sending phase hasn't finished yet, please check the contract information for more details.");

    std::vector<struct blockbase::producers> producersWhoFailedToSendIPsList = GetProducersWhoFailedToSendIPs(owner);
    auto reservedSeatsCount = std::distance(_reservedseats.begin(), _reservedseats.end());

    if (producersWhoFailedToSendIPsList.size() > 0) {
        RemoveIPsDAM(owner, producersWhoFailedToSendIPsList);
        RemoveProducersDAM(owner, producersWhoFailedToSendIPsList);
        DeleteCurrentProducerDAM(owner,producersWhoFailedToSendIPsList);
        RemoveBlockCountDAM(owner,producersWhoFailedToSendIPsList);
        ClearWarningDAM(owner,producersWhoFailedToSendIPsList);
        auto numberOfProducersRequired = info->number_of_validator_producers_required + info->number_of_history_producers_required + info->number_of_full_producers_required + reservedSeatsCount;

        if (std::distance(_producers.begin(), _producers.end()) < ceil(numberOfProducersRequired * MIN_PRODUCERS_IN_CHAIN_THRESHOLD)) {
            ChangeContractStateDAM({owner, true, false, true, false, false, false, false});
            SetEndDateDAM(owner, CANDIDATURE_TIME_ID);
            eosio::print("Sidechain paused, Candidature time started again \n");
            return;
        } else if (std::distance(_producers.begin(), _producers.end()) < ceil(numberOfProducersRequired * MIN_PRODUCERS_TO_PRODUCE_THRESHOLD)){
            ChangeContractStateDAM({owner, true, false, true, false, false, false, state->is_production_phase});
            SetEndDateDAM(owner, CANDIDATURE_TIME_ID);
            eosio::print("Candidature time started again. \n");
            return;
        }
    }

    ChangeContractStateDAM({owner, true, false, false, false, false, true, state->is_production_phase});
    AddBlockCountDAM(owner);
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

    check(state != _states.end() && state->has_chain_started == true && state->is_ip_retrieving_phase == true, "The chain is not in the IP receiving phase, please check the current state of the chain.");
    check(info != _infos.end(), "No configuration inserted, please insert the configuration first. \n");
    check(eosio::current_block_time().to_time_point().sec_since_epoch() >= info->ip_retrieval_phase_end_date_in_seconds && info->ip_retrieval_phase_end_date_in_seconds != 0, "The IP receive phase hasn't finished yet, please check the contract information for more details.");

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

[[eosio::action]] void blockbase::addcandidate(eosio::name owner, eosio::name candidate, std::string &publicKey, checksum256 secretHash, uint8_t producerType, uint32_t softwareVersion) {
    require_auth(candidate);

    stateIndex _states(_self, owner.value);
    blacklistIndex _blacklists(_self, owner.value);
    infoIndex _infos(_self, owner.value);
    reservedseatIndex _reservedseats(_self, owner.value);

    auto state = _states.find(owner.value);
    auto blackListedAccount = _blacklists.find(candidate.value);
    auto info = _infos.find(owner.value);
    auto reservedSeat = _reservedseats.find(candidate.value);

    check(blackListedAccount == _blacklists.end(), "This account is blacklisted and can't enter this sidechain.");
    check(reservedSeat == _reservedseats.end() || reservedSeat->producer_type == producerType, "This account is in reserved seats with a different producer type");

    check(state != _states.end() && state->has_chain_started == true && state->is_candidature_phase == true, "The chain is not in the candidature phase, please check the current state of the chain.");
    check(IsCandidateValid(owner, candidate), "Candidate is already a candidate, a producer, or is banned");
    check(IsPublicKeyValid(owner, publicKey), "Public key not unique in sidechain or in incorrect format. \n");
    check(IsVersionValid(owner, softwareVersion), "The software version in use is not supported by this sidechain. Candidature failed");
    eosio::asset candidateStake = blockbasetoken::get_stake(BLOCKBASE_TOKEN, owner, candidate);
    check(candidateStake.amount > 0, "No stake inserted in the sidechain. Please insert a stake first.\n");

    check(candidateStake.amount >= info->min_candidature_stake, "Stake inserted is not enough. Please insert more stake to be able to apply.");
    check(producerType == 1 || producerType == 2 || producerType == 3, "Incorrect producer type. Pleace choose a correct producer type");
    if (reservedSeat == _reservedseats.end())
    {
        check((producerType == 1 && info -> number_of_validator_producers_required != 0) || (producerType == 2  && info -> number_of_history_producers_required != 0) || (producerType == 3 && info -> number_of_full_producers_required != 0), "The producer type is not required in the given sidechain configuration. Pleace choose another type");
    }
    AddCandidateDAM(owner, candidate, publicKey, secretHash, producerType);
    eosio::print("Candidate added.");
}

[[eosio::action]] void blockbase::addencryptip(eosio::name owner, eosio::name producer, std::vector<std::string> encryptedIps) {
    require_auth(producer);

    stateIndex _states(_self, owner.value);

    auto state = _states.find(owner.value);
    check(state != _states.end() && state->has_chain_started == true && (state->is_ip_sending_phase == true || state->is_production_phase), "The chain is not in the IP sending phase, please check the current state of the chain. \n");

    ipsIndex _ips(_self, owner.value);
    auto ip = _ips.find(producer.value);
    check(ip != _ips.end(), "Producer not found.");
    
    _ips.modify(ip, producer, [&](auto &ipaddress) {
        ipaddress.encrypted_ips.clear();
        for (auto iplist : encryptedIps)
            ipaddress.encrypted_ips.push_back(iplist);
    });

    eosio::print("Information successfully inserted. \n");
}

[[eosio::action]] void blockbase::updatekey(eosio::name owner, eosio::name producer, std::string publicKey) {
    require_auth(producer);

    stateIndex _states(_self, owner.value);
    producersIndex _producers(_self, owner.value);

    auto state = _states.find(owner.value);
    auto producerInSidechain = _producers.find(producer.value);

    check(state != _states.end() && state->has_chain_started == true, "The chain is not started, please check the current state of the chain. \n");
    check(producerInSidechain != _producers.end(), "Producer not in pool.");
    
    _producers.modify(producerInSidechain, producer, [&](auto &producerI) {
        producerI.public_key = publicKey;
    });
}

[[eosio::action]] void blockbase::addsecret(eosio::name owner, eosio::name producer, checksum256 secret) {
    require_auth(producer);

    stateIndex _states(_self, owner.value);
    infoIndex _infos(_self, owner.value);
    candidatesIndex _candidates(_self, owner.value);

    auto info = _infos.find(owner.value);
    auto state = _states.find(owner.value);

    auto candidate = _candidates.find(producer.value);
    check(candidate != _candidates.end(), "Your account was not selected for the producing pool.");

    check(state != _states.end() && state->has_chain_started == true && state->is_secret_sending_phase == true, "The chain is not in the sending secret phase, please check the current state of the chain.");
    check(info != _infos.end(), "The chain doesn't have any configurations inserted. Please insert configurations to begin production.");
    check(IsSecretValid(owner, producer, secret), "Secret is invalid, please insert a valid secret.");

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

    check(state != _states.end() && state->has_chain_started == true && state->is_production_phase == true, "The chain is not in the production phase, please check the current state of the chain.");
    check(producerInSidechain != _producers.end(), "Producer not in pool.");
    check(IsProducerTurn(owner, producer), "It's not this producer turn to produce a block.");
    check(IsTimestampValid(owner, block), "Invalid timestamp in block header.");
    check(IsPreviousBlockHashAndSequenceNumberValid(owner, block), "Invalid previous blockhash or sequence number in block header.");
    check(!HasBlockBeenProduced(owner, producer), "You already produced in this time slot, wait for your turn.");
    AddBlockDAM(owner, producer, block);
    UpdateBlockCheckDAM(owner, producer, block.block_hash);
    eosio::print("Block submited with success.");
}

[[eosio::action]] void blockbase::rcandidate(eosio::name owner, eosio::name producer) {
    require_auth(producer);

    stateIndex _states(_self, owner.value);
    candidatesIndex _candidates(_self, owner.value);

    auto state = _states.find(owner.value);
    auto candidateInSidechainToRemove = _candidates.find(producer.value);

    check(state != _states.end() && state->has_chain_started == true && state->is_secret_sending_phase == false && state->is_ip_sending_phase == false && state->is_ip_retrieving_phase == false, "The chain is not in the candidature phase, please check the current state of the chain. \n");
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
    check(_producers.find(producer.value) == _producers.end() && _candidates.find(producer.value) == _candidates.end(), "This account is currently a candidate or a producer.");
    check(_blacklists.find(producer.value) != _blacklists.end(), "This user is not in the blacklist of this smart contract.");

    _blacklists.erase(blackListedProducer);
}

[[eosio::action]] void blockbase::iamready(eosio::name owner, eosio::name producer) {
    require_auth(producer);
    producersIndex _producers(_self, owner.value);
    auto producerInSidechain = _producers.find(producer.value);
    check(producerInSidechain != _producers.end(), "This producer doesn't exist.");
    check(producerInSidechain->is_ready_to_produce == false, "This producer is already ready.");
    _producers.modify(producerInSidechain, producer, [&](auto &producerI) {
        producerI.is_ready_to_produce = true;
    });
    eosio::print("Producer ", producer, "is ready and will start producting.");
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
              lastblock.back().timestamp + 259200 < eosio::current_block_time().to_time_point().sec_since_epoch(), // If the chain has no new blocks for 3 days the producer can leave the chain.
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
    verifysigIndex _verifysig(_self, owner.value);

    auto info = _infos.find(owner.value);
    auto state = _states.find(owner.value);
    auto client = _clients.find(owner.value);

    check(state != _states.end() && state->has_chain_started == true && state->is_production_phase == true, "The chain is not in production state, please check the current state of the chain. \n");
    check(info != _infos.end(), "The chain doesn't have any configurations inserted. Please insert configurations to begin production. \n");

    std::vector<blockbase::producers> readyProducerslist = GetReadyProducers(owner);
    if (readyProducerslist.size() > 0) {
        auto blockCountForComputation = 0;

        for (auto count : _blockscount) {
            blockCountForComputation += count.num_blocks_produced;
            blockCountForComputation += count.num_blocks_failed;
        }

        if (blockCountForComputation >= info->num_blocks_between_settlements) {
            eosio::print("Computation time... \n");
            RunSettlement(owner);
        }

        if (blockCountForComputation == 1)
        {
            check(IsRequesterStakeEnough(owner), "Not enough stake to continue running chain");
        }

        readyProducerslist = GetReadyProducers(owner); // The ready producers list can change in the settlement.
        auto currentProducer = _currentprods.begin();

        if (readyProducerslist.size() > 0) {
            struct blockbase::producers nextproducer = GetNextProducer(owner);

            if (currentProducer == _currentprods.end() || (currentProducer->production_start_date_in_seconds + info->block_time_in_seconds) <= eosio::current_block_time().to_time_point().sec_since_epoch()) {
                if (currentProducer != _currentprods.end()) {
                    UpdateBlockCount(owner, currentProducer->producer, currentProducer->production_start_date_in_seconds);
                }
                
                auto deleteitr = _verifysig.begin();
                while (deleteitr != _verifysig.end())
                    deleteitr = _verifysig.erase(deleteitr);
                
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
    check(std::distance(_blockheaders.begin(), _blockheaders.end()) != 0, "No blockheaders in table");

    auto blockToValidate = --_blockheaders.end();

    if (lastblock.size() > 0)
        check(lastblock.back().timestamp + (info->block_time_in_seconds) < eosio::current_block_time().to_time_point().sec_since_epoch(), "Time to verify block already passed");

    if (std::distance(_blockheaders.begin(), _blockheaders.end()) > 0) {
        if (blockToValidate->block_hash == blockHash && blockToValidate->is_verified == false && blockToValidate->is_latest_block == false) {
            
            _blockheaders.modify(blockToValidate, same_payer, [&](auto &newBlockI) {
                newBlockI.is_verified = true;
                newBlockI.is_latest_block = true;
            });
            
            if (lastblock.size() > 0) {
                auto blockToModify = _blockheaders.find(lastblock.back().sequence_number);
                _blockheaders.modify(blockToModify, same_payer, [&](auto &newBlockI) {
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
    warningsIndex _warnings(_self, owner.value);

    for (auto producer : _producers) {
        auto producerWarningId = GetSpecificProducerWarningId(owner, producer.key, WARNING_TYPE_PUNISH);
        auto blackListedProducer = _blacklists.find(producer.key.value);
        if (blackListedProducer == _blacklists.end() && producerWarningId != -1) {
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
    auto histval = _histval.find(producer.value);
    check(histval == _histval.end(), "Validation request for this producer already inserted");
    _histval.emplace(owner, [&](auto &historyValidationI) {
        historyValidationI.key = producer;
        historyValidationI.block_hash = blockHash;
    });
}

[[eosio::action]] void blockbase::addblckbyte(eosio::name owner, eosio::name producer, std::string byteInHex, std::vector<char> packedTransaction) {
    require_auth(producer);
    histvalIndex _histval(_self, owner.value);
    auto histval = _histval.find(producer.value);
    check(histval != _histval.end(), "No validation request for this producer inserted");

    _histval.modify(histval, producer, [&](auto &historyValidationI) {
        historyValidationI.block_byte_in_hex = byteInHex;
        historyValidationI.packed_transaction = packedTransaction;
        historyValidationI.verify_signatures.clear();
        historyValidationI.signed_producers.clear();
    });
}

[[eosio::action]] void blockbase::addhistsig(eosio::name owner, eosio::name producer, eosio::name producerToValidade, std::string verifySignature, std::vector<char> packedTransaction) {
    require_auth(producer);
    histvalIndex _histval(_self, owner.value);
    producersIndex _producers(_self, owner.value);
    auto histval = _histval.find(producerToValidade.value);
    auto producerInTable = _producers.find(producer.value);
    check(producerInTable != _producers.end(), "Not a producer in this chain to be able to run action");
    check(histval != _histval.end(), "No validation request for this producer inserted");
    check(std::equal(histval->packed_transaction.begin(), histval->packed_transaction.end(), packedTransaction.begin()), "Packed transaction doesn't match history validation entry");
    check(std::find(histval->signed_producers.begin(), histval->signed_producers.end(), producer) == histval->signed_producers.end(), "Producer already inserted signature");
    check(std::find(histval->verify_signatures.begin(), histval->verify_signatures.end(), verifySignature) == histval->verify_signatures.end(), "Signature already inserted");

    _histval.modify(histval, producer, [&](auto &historyValidationI) {
        historyValidationI.verify_signatures.push_back(verifySignature);
        historyValidationI.signed_producers.push_back(producer);
    });
}

[[eosio::action]] void blockbase::histvalidate(eosio::name owner, eosio::name producer, std::string blockHash) {
    require_auth(owner);
    histvalIndex _histval(_self, owner.value);
    warningsIndex _warnings(_self, owner.value);
    auto histval = _histval.find(producer.value);
    if (histval != _histval.end()) {
        check(histval->block_hash == blockHash, "Sent block hash is not valid");
        auto producerSpecificWarningId = GetSpecificProducerWarningId(owner, producer, WARNING_TYPE_HISTORY_VALIDATION_FAILED);
        if (producerSpecificWarningId != -1)
           ClearWarningDAM(owner, producer, producerSpecificWarningId);
        histval = _histval.erase(histval);
    }
}

[[eosio::action]] void blockbase::addaccperm(eosio::name owner, eosio::name account, std::string publicKey, std::string permissions) {
    require_auth(owner);
    accpermIndex _accperm(_self, owner.value);
    auto accountInTable = _accperm.find(account.value);
    check(accountInTable == _accperm.end(), "Permissions for this account already inserted");
    _accperm.emplace(owner, [&](auto &accpermI) {
        accpermI.key = account;
        accpermI.public_key = publicKey;
        accpermI.permissions = permissions;
    });
}

[[eosio::action]] void blockbase::remaccperm(eosio::name owner, eosio::name account) {
    require_auth(owner);
    accpermIndex _accperm(_self, owner.value);
    auto accountInTable = _accperm.find(account.value);
    check(accountInTable != _accperm.end(), "Account permissions not found");
    _accperm.erase(accountInTable);
}

[[eosio::action]] void blockbase::addversig(eosio::name owner, eosio::name account, std::string blockHash, std::string verifySignature, std::vector<char> packedTransaction) {
    require_auth(account);
    verifysigIndex _verifysig(_self, owner.value);
    auto sigInTable = _verifysig.find(account.value);
    check(sigInTable == _verifysig.end(), "Verify signature already inserted");
    _verifysig.emplace(account, [&](auto &versigI) {
        versigI.key = account;
        versigI.block_hash = blockHash;
        versigI.verify_signature = verifySignature;
        versigI.packed_transaction = packedTransaction;
    });
}

[[eosio::action]] void blockbase::addreseats(eosio::name owner, std::vector<blockbase::reservedseat> seatsToAdd) {
    require_auth(owner);
    blacklistIndex _blacklists(_self, owner.value);
    reservedseatIndex _reservedseats(_self, owner.value);
    stateIndex _states(_self, owner.value);

    auto chainState = _states.find(owner.value);

    check(!seatsToAdd.empty(), "No account names to submited to add to the sidechain reserved seats");
    check(chainState != _states.end() && chainState->has_chain_started == true && chainState->is_secret_sending_phase == false && chainState->is_ip_sending_phase == false && chainState->is_ip_retrieving_phase == false, "Chain is not in the right state. Try again when its candidature time or production time");
    
    for(auto seatToAdd : seatsToAdd) {
        auto blacklist = _blacklists.find(seatToAdd.key.value);
        auto reservedSeat = _reservedseats.find(seatToAdd.key.value);
        if(blacklist == _blacklists.end() && reservedSeat == _reservedseats.end() && is_account(seatToAdd.key)) {
            _reservedseats.emplace(owner, [&](auto &reservedSeatI){
                reservedSeatI.key = seatToAdd.key;
                reservedSeatI.producer_type = seatToAdd.producer_type;
            });
        }
    }
}

[[eosio::action]] void blockbase::rreservseats(eosio::name owner, std::vector<eosio::name> seatsToRemove) {
    require_auth(owner);
    stateIndex _states(_self, owner.value);
    reservedseatIndex _reservedseats(_self, owner.value);
    
    auto chainState = _states.find(owner.value);
    check(!seatsToRemove.empty(), "No account names to submit to remove from sidechain reserved seats");
    check(chainState != _states.end() && chainState->has_chain_started == true && chainState->is_secret_sending_phase == false && chainState->is_ip_sending_phase == false && chainState->is_ip_retrieving_phase == false, "Chain is not in the right state. Try again when its candidature time or production time");
    
    for(auto seatToRemove : seatsToRemove) {
        auto reservedSeat = _reservedseats.find(seatToRemove.value);
        if(reservedSeat != _reservedseats.end() && is_account(seatToRemove))  
            _reservedseats.erase(reservedSeat); 
    }
}

[[eosio::action]] void blockbase::exitrequest(eosio::name owner, eosio::name account) {
    require_auth(account);
    producersIndex _producers(_self, owner.value);
    auto producerInTable = _producers.find(account.value);
    check(producerInTable != _producers.end(), "Producer is not in this sidechain" );
    check(producerInTable -> work_duration_in_seconds == std::numeric_limits<uint32_t>::max(), "This producer has already submitted an exit request");
    
    _producers.modify(producerInTable, account, [&](auto &producerI) {
        producerI.work_duration_in_seconds = eosio::current_block_time().to_time_point().sec_since_epoch() + ONE_DAY_IN_SECONDS;
    });
}

[[eosio::action]] void blockbase::alterconfig(eosio::name owner, blockbase::configchange infoChangeJson) {
    require_auth(owner);

    stateIndex _states(_self, owner.value);
    producersIndex _producers(_self, owner.value);
    reservedseatIndex _reservedseats(_self, owner.value);

    auto reservedSeatsCount = std::distance(_reservedseats.begin(), _reservedseats.end());
    auto state = _states.find(owner.value);

    check(infoChangeJson.key.value == owner.value, "Account isn't the same account as the sidechain owner.");

    check(state != _states.end() && state->has_chain_started == true && state->is_configuration_phase == false && state->is_secret_sending_phase == false && state->is_ip_sending_phase == false && state->is_ip_retrieving_phase == false , "This sidechain is in a state that doesn't allow configuration changes");
    check(IsConfigurationChangeValid(infoChangeJson, reservedSeatsCount), "The configuration changes inserted is incorrect or not valid, please insert it again.");

    UpdateChangeConfigDAM(owner, infoChangeJson);
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
    histvalIndex _histval(_self, owner.value);
    verifysigIndex _verifysig(_self, owner.value);
    versionIndex _version(_self, owner.value);
    warningsIndex _warnings(_self, owner.value);
    reservedseatIndex _reservedseats(_self, owner.value);

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

    auto itr8 = _histval.begin();
    while (itr8 != _histval.end())
        itr8 = _histval.erase(itr8);

    auto itr9 = _verifysig.begin();
    while (itr9 != _verifysig.end())
        itr9 = _verifysig.erase(itr9);

    auto itr10 = _version.begin();
    while (itr10 != _version.end())
        itr10 = _version.erase(itr10);

    auto itr11 = _warnings.begin();
    while (itr11 != _warnings.end())
        itr11 = _warnings.erase(itr11);

    auto itr12 = _reservedseats.begin();
    while (itr12 != _reservedseats.end())
        itr12 = _reservedseats.erase(itr12);

    eosio::print("Service Ended. \n");
}

#pragma endregion