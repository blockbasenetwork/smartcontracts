#include <eosio/eosio.hpp>
#include <eosio/print.hpp>
#include <eosio/transaction.hpp>
#include <eosio/asset.hpp>
#include <eosio/action.hpp>
#include <cmath>

#include <blockbasetoken/blockbasetoken.hpp>
#include <blockbase/blockbase.hpp>

#include <blockbase/authorizations.hpp>
#include <blockbase/candidature.hpp>
#include <blockbase/consensus.hpp>
#include <blockbase/blockproduce.hpp>
#include <blockbase/payments.hpp>
#include <blockbase/punishment.hpp>
#include <blockbase/service.hpp>

    #pragma region State Actions

    [[eosio::action]] 
    void blockbase::startchain(eosio::name owner, std::string publickey){
        require_auth(owner);

        clientIndex _clients(_self, owner.value);
        stateIndex _states(_self, owner.value);

        auto client = _clients.find(owner.value);
        auto state = _states.find(owner.value);

        check(client == _clients.end(), "Client information already inserted, the chain has started.");
        check(state == _states.end(), "Chain status are already created.");
        check(ispublickeyvalid(publickey), "Public key is invalid, please insert a correct public key.");

        changestate({owner, true, false, false, false, false, false, false});

        _clients.emplace(owner, [&](auto &clientI) {
            clientI.key = owner;
            clientI.publickey = publickey;
        });
        eosio::print("Chain started. You can now insert your configurations. \n");
    }

    [[eosio::action]] 
    void blockbase::configchain(eosio::name owner, blockbase::contractinfo infojson){
        require_auth(owner);

        clientIndex _clients(_self, owner.value);
        stateIndex _states(_self, owner.value);
        producersIndex _producers(_self, owner.value);
        auto state = _states.find(owner.value);
        check(state != _states.end() && state -> startchain != false && state -> productiontime == false, "This sidechain hasnt't been created yet, please create it first.");
        check(isconfigvalid(infojson), "The configurantion inserted is incorrect or not valid, please insert it again.");
        eosio::asset cstake = blockbasetoken::get_stake(BLOCKBASE_TOKEN, owner, owner);
        check(cstake.amount > MIN_STAKE_FOR_CLIENT, "No stake inserted or the amount is not valid. Please insert your stake and configure the chain again. \n");

        changestate({owner, true, true, false, false, false, false, false});
        infomanage(owner, infojson);
        if(std::distance(_producers.begin(), _producers.end()) > 0){
            deleteblockcount(owner);
            deleteips(owner);
            deleteprods(owner);
        } 
        eosio::print("Information inserted. \n");
    }

    [[eosio::action]] 
    void blockbase::startcandtime(eosio::name owner) {
        require_auth(owner);
        eosio::print("Configuration time ended. \n");

        infoIndex _infos(_self, owner.value);
        stateIndex _states(_self, owner.value);
        auto info = _infos.find(owner.value);
        auto state = _states.find(owner.value);

        check(iscandidatetime(owner), "The chain is not in the correct state, please check the current state of the chain. \n");
        check(info != _infos.end(), "No configuration inserted, please insert the configuration first. \n");

        setenddate(owner, CANDIDATURE_TIME_ID);

        changestate({owner, true, false, true, false, false, false, state -> productiontime});

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
        check(state != _states.end() && state -> startchain != false && state -> candidaturetime != false, "The chain is not in the correct state, please check the current state of the chain.");
        auto totalsize = std::distance(_producers.begin(), _producers.end()) + std::distance(_candidates.begin(), _candidates.end());
        if (comparenumbers(totalsize, info -> requirednumberofproducers)) {
            eosio::print("Starting candidature time again... \n");
            setenddate(owner, CANDIDATURE_TIME_ID);
            changestate({owner, true, false, true, false, false, false, state -> productiontime});
            return;
        }

        changestate({owner, true, false, false, true, false, false, state -> productiontime});

        setenddate(owner, SECRET_TIME_ID);

        eosio::print("Start Secret send time. \n");
    }

    [[eosio::action]] 
    void blockbase::startsendtime(eosio::name owner) {
        require_auth(owner);
        eosio::print("Secret send time ended. \n");

        producersIndex _producers(_self, owner.value);
        stateIndex _states(_self, owner.value);
        infoIndex _infos(_self, owner.value);
        candidatesIndex _candidates(_self, owner.value);

        auto state = _states.find(owner.value);
        auto info = _infos.find(owner.value);

        check(info != _infos.end(), "No configuration inserted, please insert the configuration first. \n");
        check(state != _states.end() && state -> startchain != false && state -> secrettime != false, "The chain is not in the correct state, please check the current state of the chain. \n");

        auto producersize = std::distance(_producers.begin(), _producers.end());

        std::vector<struct blockbase::candidates> finalcandidatelist = choosecandidates(owner);
        if(comparenumbers(producersize + finalcandidatelist.size(), info -> requirednumberofproducers)) {
            if(comparenumbers(producersize, ceil((info -> requirednumberofproducers) * PRODUCERS_IN_CHAIN_THRESHOLD))){
                cancel_deferred(owner.value + CHANGE_PRODUCER_ID);
                changestate({owner, true, false, false, false, false, false, false});
                deleteblockcount(owner);
                deleteips(owner);
                deleteprods(owner);
                authassign(owner, VERIFY_PERMISSION_NAME, eosio::name("active"), thresholdcalc(std::distance(_producers.begin(), _producers.end())));
                eosio::print("Not enough candidates, configure the chain again. \n");
            } else {
                changestate({owner, true, false, true, false, false, false, state -> productiontime});
                cancel_deferred(owner.value + SEND_TIME_ID);
                cancel_deferred(owner.value + SECRET_TIME_ID);
                eosio::print("Starting candidature time again... \n");
                setenddate(owner, CANDIDATURE_TIME_ID);
                return;
            }
        } else {
            for (auto candidate : finalcandidatelist) {
                addprod(owner, candidate);
                addpublickey(owner, candidate.key, candidate.publickey);
                rcandidate(owner, candidate.key);
                authassign(owner, VERIFY_PERMISSION_NAME, eosio::name("active"), thresholdcalc(std::distance(_producers.begin(), _producers.end())));
            }
            eosio::print("Producers sucessfully inserted. \n");
            setenddate(owner, SEND_TIME_ID);

            changestate({owner, true, false, false, false, true, false, state -> productiontime});
            
            eosio::print("Start send time. \n");
        }
    }

    [[eosio::action]]
    void blockbase::startrectime(eosio::name owner) { 
        require_auth(owner);
        eosio::print("Send time ended. \n");
        producersIndex _producers(_self, owner.value);
        infoIndex _infos(_self, owner.value);
        stateIndex _states(_self, owner.value);

        auto state = _states.find(owner.value);
        auto info = _infos.find(owner.value);

        check(state != _states.end() && state -> startchain != false && state -> ipsendtime != false, "The chain is not in the correct state, please check the current state of the chain.");
        check(info != _infos.end(), "No configuration inserted, please insert the configuration first.");

        std::vector<struct blockbase::producers> finalproducerlist = checksendprods(owner);
        if(finalproducerlist.size() > 0){
            deleteblockcount(owner);
            deleteips(owner, finalproducerlist);
            deleteprods(owner, finalproducerlist);

            if(comparenumbers(std::distance(_producers.begin(), _producers.end()), ceil(info -> requirednumberofproducers * PRODUCERS_IN_CHAIN_THRESHOLD))) {
                changestate({owner, true, false, false, false, false, false, false});
                eosio::print("Configure chain again. \n");
                return;
            } else {
                changestate({owner, true, false, true, false, false, false, state -> productiontime});

                eosio::print("Candidature time started again. \n");
                cancel_deferred(owner.value + SEND_TIME_ID);
                setenddate(owner, CANDIDATURE_TIME_ID);
                return;
            }
        } 
        
        changestate({owner, true, false, false, false, false, true, state -> productiontime});
        startcount(owner, false);
        setenddate(owner, RECEIVE_TIME_ID);
        eosio::print("Start receive time. \n");
    }

    [[eosio::action]] 
    void blockbase::prodtime(eosio::name owner) {
        require_auth(owner);
        eosio::print("End receive time. \n");

        infoIndex _infos(_self, owner.value);
        stateIndex _states(_self, owner.value);
        producersIndex _producers(_self, owner.value);
        auto state = _states.find(owner.value);
        auto info = _infos.find(owner.value);
        check(state != _states.end() && state->startchain != false && state -> ipreceivetime != false, "The chain is not in the correct state, please check the current state of the chain. \n");
        check(info != _infos.end(), "No configuration inserted, please insert the configuration first. \n");

        changestate({owner, true, false, false, false, false, false, true});

        if (state->productiontime) return;

        linkauth(owner, VERIFY_PERMISSION_ACTION, VERIFY_PERMISSION_NAME);

        eosio::print("And let the production begin! \n");
        eosio::print("Inserting current producer. \n");
        currentprodIndex _currentprods(_self, owner.value);
        std::vector<blockbase::producers> producerslist = getreadyprods(owner);
        if (std::distance(_currentprods.begin(), _currentprods.end()) == 0 && producerslist.size() > 0) {
            struct blockbase::producers nextproducer = getnextprod(owner);
            if(nextproducer.isready) nextcurrentprod(owner, nextproducer.key);
        }
        cancel_deferred(owner.value + CHANGE_PRODUCER_ID);
    }

    #pragma endregion
    #pragma region User Actions
    [[eosio::action]] 
    void blockbase::addcandidate(eosio::name owner, eosio::name candidate, uint64_t &worktimeinseconds, std::string &publickey, checksum256 secrethash) {
        require_auth(candidate);

        stateIndex _states(_self, owner.value);
        blacklistIndex _blacklists(_self, owner.value);
        infoIndex _infos(_self, owner.value);

        auto state = _states.find(owner.value);
        auto blacklisted = _blacklists.find(candidate.value);
        auto info = _infos.find(owner.value);

        check(blacklisted == _blacklists.end(), "This account is blacklisted and can't enter this sidechain. \n");
        check(state != _states.end() && state -> startchain != false && state -> candidaturetime != false, "The chain is not in the correct state, please check the current state of the chain. \n");
        check(iscandidatevalid(owner, candidate, worktimeinseconds), "Candidature is invalid, please check the inserted values. \n");
        check(ispublickeyvalid(publickey), "Incorrect format in public key, try inserting again. \n");
        eosio::asset candidatestake = blockbasetoken::get_stake(BLOCKBASE_TOKEN, owner, candidate);
        check(candidatestake.amount > 0, "No stake inserted in the sidechain. Please insert a stake first.\n");
        check(candidatestake.amount > info -> minimumcandidatestake, "Stake inserted is not enough. Please insert more stake to be able to apply.\n");
        insertcandidate(owner, candidate, worktimeinseconds, publickey, secrethash);
        eosio::print("Candidate added. \n");
    }

    [[eosio::action]] 
    void blockbase::addencryptip(eosio::name owner, eosio::name producer, std::vector<std::string> encryptedips) {
        require_auth(producer);

        stateIndex _states(_self, owner.value);
        infoIndex _infos(_self, owner.value);

        auto state = _states.find(owner.value);
        check(state != _states.end() && state -> startchain != false && state -> ipsendtime != false, "The chain is not in the correct state, please check the current state of the chain. \n");

        ipsIndex _ips(_self, owner.value);
        auto ip = _ips.find(producer.value);

        _ips.modify(ip, producer, [&](auto &ipaddress) 
        {
            ipaddress.encryptedips.clear();
            for (auto iplist : encryptedips) ipaddress.encryptedips.push_back(iplist);
        });

        eosio::print("Information successfully inserted. \n");
    }

    [[eosio::action]]
     void blockbase::addsecret(eosio::name owner, eosio::name producer, checksum256 secret) {
        require_auth(producer);

        stateIndex _states(_self, owner.value);
        infoIndex _infos(_self, owner.value);
        candidatesIndex _candidates(_self, owner.value);

        auto info = _infos.find(owner.value);
        auto state = _states.find(owner.value);
        auto candidate = _candidates.find(producer.value);

        check(state != _states.end() && state -> startchain != false && state -> secrettime == true, "The chain is not in the correct state, please check the current state of the chain. \n");
        check(info != _infos.end(), "The chain doesn't have any configurations inserted. Please insert configurations to begin production. \n");
        check(issecretvalid(owner, producer, secret), "Secret is invalid, please insert a valid secret");
        check(candidate != _candidates.end(), "Your account was not selected for the producing pool. \n");

        _candidates.modify(candidate, producer, [&](auto &candidateI) {
            candidateI.secret = secret;
        });
        eosio::print("Secret inserted. \n");
    }

    [[eosio::action]]
    void blockbase::addblock(eosio::name owner, eosio::name producer, blockbase::blockheaders block) {
        require_auth(producer);
        
        stateIndex _states(_self, owner.value);
        producersIndex _producers(_self, owner.value);

        auto state = _states.find(owner.value);
        auto producersinpool = _producers.find(producer.value);
        check(state != _states.end() && state -> startchain != false && state -> productiontime != false, "The chain is not in the correct state, please check the current state of the chain. \n");
        check(producersinpool != _producers.end(), "Producer not in pool. \n");
        check(isprodtime(owner, producer), "It's not this producer turn to produce a block. \n");
        check(isblockvalid(owner, block), "Invalid Block. \n");
        check(isblockprod(owner, producer), "You already produced in this time slot, wait for your turn. \n");
        insertblock(owner, producer, block);
        eosio::print("Block submited with success. \n");
    }

    [[eosio::action]]
    void blockbase::rcandidate(eosio::name owner, eosio::name producer) {
        require_auth(owner);

        stateIndex _states(_self, owner.value);
        candidatesIndex _candidates(_self, owner.value);

        auto state = _states.find(owner.value);
        auto candidatetoremove = _candidates.find(producer.value);
        check(state != _states.end() && state -> startchain != false, "The chain is not in the correct state, please check the current state of the chain. \n");
        check(candidatetoremove != _candidates.end(), "Candidate can't be removed. Candidate doesn't exist in the candidate list. \n");

        _candidates.erase(candidatetoremove);
        eosio::print("Candidate removed. \n");
    }

    [[eosio::action]]
     void blockbase::resetreward(eosio::name owner, eosio::name producer){
        require_auth(BLOCKBASE_TOKEN);
        rewardsIndex _rewards(_self, producer.value);
        producersIndex _producers(_self, owner.value);

        auto rewardsforproducer = _rewards.find(producer.value);
        auto producerI = _producers.find(producer.value);
        if(rewardsforproducer != _rewards.end() && rewardsforproducer -> reward > 0){
            _rewards.modify(rewardsforproducer, producer, [&](auto &rewardI) {
                rewardI.reward = 0;
            });
        }
        if(producerI == _producers.end()) _rewards.erase(rewardsforproducer);
    }

    [[eosio::action]]
    void blockbase::blistremoval(eosio::name owner, eosio::name producer) {
        require_auth(owner);
        producersIndex _producers(_self, owner.value);
        candidatesIndex _candidates(_self, owner.value);
        blacklistIndex _blacklists(_self, owner.value);
        auto producerblacklisted = _blacklists.find(producer.value);
        check(_blacklists.find(producer.value) != _blacklists.end(), "This user is not in the blacklist of this smart contract. \n");
        check(_producers.find(producer.value) == _producers.end() && _candidates.find(producer.value) == _candidates.end(), "This producer is currently in producer pool \n");
        _blacklists.erase(producerblacklisted);
    }

    [[eosio::action]]
     void blockbase::iamready(eosio::name owner, eosio::name producer) {
        require_auth(producer);
        producersIndex _producers(_self, owner.value);
        currentprodIndex _currentprods(_self, owner.value);
        auto producerrecord = _producers.find(producer.value);
        check(producerrecord != _producers.end(), "This producer doesn't exist. \n");
        check(producerrecord -> isready != true, "This producer is already ready. \n");
        _producers.modify(producerrecord, producer, [&](auto &producerI) {
            producerI.isready = true;
        });
        eosio::print("Producer ", producer, "is ready and will start producting. \n");
    }

    [[eosio::action]]
     void blockbase::exitrequest(eosio::name owner, eosio::name producer){
        require_auth(producer);
        producersIndex _producers(_self, owner.value);
        auto producerI = _producers.find(producer.value);
        check(producerI != _producers.end(), "Producer doesn't exist. \n");
        check(eosio::current_block_time().to_time_point().sec_since_epoch() <= (((producerI -> worktimeinseconds) + (producerI -> startinsidechaindate)) - MIN_WORKDAYS_IN_SECONDS), "  Producer is leaving in less then a day. \n");
        _producers.modify(producerI, producer, [&](auto &producerIT) {
            producerIT.worktimeinseconds = eosio::current_block_time().to_time_point().sec_since_epoch() - MIN_WORKDAYS_IN_SECONDS;
        });
        eosio::print("Exit request sucessfull, producer will leave in one day. \n");
    }

    [[eosio::action]] 
    void blockbase::changecprod(eosio::name owner) {
        require_auth(owner);

        eosio::print("Updating current producer... \n");

        infoIndex _infos(_self, owner.value);
        stateIndex _states(_self, owner.value);
        clientIndex _clients(_self, owner.value);
        currentprodIndex _currentprods(_self, owner.value);
        producersIndex _producers(_self, owner.value);
        blockscountIndex _blockscount(_self, owner.value);

        auto info = _infos.find(owner.value);
        auto state = _states.find(owner.value);
        auto client = _clients.find(owner.value);

        check(state != _states.end() && state -> startchain != false && state -> productiontime != false, "The chain is not in the correct state, please check the current state of the chain. \n");
        check(info != _infos.end(), "The chain doesn't have any configurations inserted. Please insert configurations to begin production. \n");
        check(client != _clients.end(), "No client information in the chain. Please insert the needed information. \n");

        std::vector<blockbase::producers> producerslist = getreadyprods(owner);
        if(producerslist.size() > 0) {
            auto currentproducer = _currentprods.begin();
            
            blockcount(owner, currentproducer -> producer);
            auto blockcomputation = 0;
            for(auto count : _blockscount) {
                blockcomputation += count.blocksproduced;
                blockcomputation += count.blocksfailed;
            }

            if(blockcomputation >= info -> blocksbetweensettlement) {
                eosio::print("Computation time... \n");
                computation(owner);
            }
            producerslist = getreadyprods(owner);
            if(producerslist.size() > 0) {
                struct blockbase::producers nextproducer = getnextprod(owner);
                if((currentproducer -> startproductiontime + (info -> blocktimeduration) / 2) <= eosio::current_block_time().to_time_point().sec_since_epoch()) {
                    nextcurrentprod(owner, nextproducer.key);
                    eosio::print("Current producer updated. \n");
                } else eosio::print("Same producer producing blocks. \n");
            }
        }
        decisionmark(owner);
    }
    
    [[eosio::action]] 
    void blockbase::verifyblock(eosio::name owner, eosio::name producer, std::string blockhash){
        require_auth(owner);

        blockheadersIndex _blockheaders(_self, owner.value);
        currentprodIndex _currentprods(_self, owner.value);
        std::vector<blockbase::blockheaders> lastblock = blockbase::getlastblock(owner);
        auto blocktovalidate = _blockheaders.find((--_blockheaders.end()) -> sequencenumber);
        if(std::distance(_blockheaders.begin(), _blockheaders.end()) > 0){
            if (blocktovalidate -> blockhash == blockhash && blocktovalidate -> isverified == false && blocktovalidate -> islastblock == false) {
                _blockheaders.modify(blocktovalidate, _self, [&](auto &newblockI) {
                    newblockI.isverified = true;
                    newblockI.islastblock = true;
                });
                if (lastblock.size() > 0) {
                    auto blocktomodify = _blockheaders.find(lastblock.back().sequencenumber);
                    _blockheaders.modify(blocktomodify, _self, [&](auto &newblockI) {
                        newblockI.islastblock = false;
                    });
                }
                eosio::print("Block validated. \n");
                return;
            }
        }
    }

    [[eosio::action]]
    void blockbase::endservice(eosio::name owner) {
        require_auth(owner);
        eosio::print("Ending Service. \n");
        infoIndex _infos(_self, owner.value);
        stateIndex _states(_self, owner.value);
        clientIndex _clients(_self, owner.value);
        currentprodIndex _currentprods(_self, owner.value);
        producersIndex _producers(_self, owner.value);
        candidatesIndex _candidates(_self, owner.value);
        blacklistIndex _blacklists(_self, owner.value);
        rewardsIndex _rewards(_self, owner.value);

        deleteprods(owner);
        deleteips(owner);
        deleteblockcount(owner);

        auto itr = _infos.begin();
        while (itr != _infos.end()) itr = _infos.erase(itr);
        
        auto itr1 = _states.begin();
        while (itr1 != _states.end()) itr1 = _states.erase(itr1);
    
        auto itr2 = _clients.begin();
        while (itr2 != _clients.end()) itr2 = _clients.erase(itr2);

        auto itr3 = _rewards.begin();
        while (itr3 != _rewards.end()) itr3 = _rewards.erase(itr3);
    
        auto itr4 = _candidates.begin();
        while (itr4 != _candidates.end()) itr4 = _candidates.erase(itr4);
        
        auto itr5 = _blacklists.begin();
        while (itr5 != _blacklists.end()) itr5 = _blacklists.erase(itr5);
        
        auto itr6 = _currentprods.begin();
        while (itr6 != _currentprods.end()) itr6 = _currentprods.erase(itr6);
        
        eosio::print("Service Ended. \n");
    }

    #pragma endregion