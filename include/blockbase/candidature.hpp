#pragma region Validation Methods
    
    bool blockbase::comparenumbers(uint16_t variablenumber, uint16_t fixednumber) {
        return variablenumber < fixednumber;
    }

    bool blockbase::issecretvalid(eosio::name owner, eosio::name name, checksum256 secret) {
        candidatesIndex _candidates(_self, owner.value);
        auto candidate = _candidates.find(name.value);
        if(sizeof(secret) <= 0) return false;
        auto hash = candidate -> secrethash;
        auto secretArray = secret.extract_as_byte_array();
        checksum256 result = sha256((char *) &secretArray, sizeof(secretArray));
        return result == hash;
    }

    bool blockbase::ispublickeyvalid(std::string publickey){	
        return publickey.size() == 53 && publickey.substr(0,3) == "EOS";
    }

    bool blockbase::isconfigvalid(blockbase::contractinfo info) {
        if(info.candidaturetime > MAX_CANDIDATURE_TIME || info.candidaturetime < MIN_CANDIDATURE_TIME) return false;
        if(info.ipsendtime > MAX_IP_SEND_TIME || info.ipsendtime < MIN_IP_SEND_TIME) return false; 
        if(info.ipreceivetime > MAX_IP_SEND_TIME || info.ipreceivetime < MIN_IP_SEND_TIME) return false;
        if(info.sizeofblockinbytes > MAX_BLOCK_SIZE) return false;
        if(info.requirednumberofproducers > MAX_PRODUCERS || info.requirednumberofproducers < MIN_PRODUCERS) return false;
        return info.paymentperblock >= MIN_PAYMENT && info.minimumcandidatestake >= MIN_CANDIDATE_STAKE 
            && info.blocksbetweensettlement <= MAX_BLOCK_TIME_BEETWEEN_BLOCKS;
    }

    bool blockbase::iscandidatetime(eosio::name owner) {
        stateIndex _states(_self, owner.value);
        auto state = _states.find(owner.value);
        return ((state -> configtime == true && state -> productiontime == false) || (state -> configtime == true && state -> productiontime == true)) && state -> startchain == true && state != _states.end();
    }

    bool blockbase::iscandidatevalid(eosio::name owner, eosio::name producer, uint64_t worktimeinseconds) {
        producersIndex _producers(_self, owner.value);
        blacklistIndex _blacklists(_self, owner.value);
        candidatesIndex _candidates(_self, owner.value);
        return (_producers.find(producer.value) == _producers.end()) && (_candidates.find(producer.value) == _candidates.end()) && (_blacklists.find(producer.value) == _blacklists.end()) && (worktimeinseconds >= MIN_WORKDAYS_IN_SECONDS);
    }

    uint8_t blockbase::numberofips(float numberofproducers) {
        if(numberofproducers == 1) return 0;
        return ceil(numberofproducers/4.0);
    }

    std::vector<struct blockbase::candidates> blockbase::choosecandidates(eosio::name owner) {
        infoIndex _infos(_self, owner.value);
        candidatesIndex _candidates(_self, owner.value);
        producersIndex _producers(_self, owner.value);
        
        std::vector<struct blockbase::candidates> finalcandlist;

        auto info = _infos.get(owner.value);
        auto producersinpool = std::distance(_producers.begin(), _producers.end());

        for(auto candidate : _candidates){
            if(issecretvalid(owner, candidate.key, candidate.secret)) finalcandlist.push_back(candidate);
        }
        struct StakeComparator{
            explicit StakeComparator(eosio::name sidechain_) : sidechain(sidechain_) {}

            bool operator()(blockbase::candidates cand1, blockbase::candidates cand2) const{
                eosio::asset cstake1 = blockbasetoken::get_stake(eosio::name("bbtoken"), sidechain, cand1.key);
                eosio::asset cstake2 = blockbasetoken::get_stake(eosio::name("bbtoken"), sidechain, cand2.key);
                return cstake1 < cstake2;
            }
            eosio::name sidechain;
        };

        std::sort(finalcandlist.begin(), finalcandlist.end(), StakeComparator(_self));

        while((finalcandlist.size() + producersinpool) > info.requirednumberofproducers){
            for(auto i = 0; i < finalcandlist.size(); i += 2){
                std::array<uint8_t, 64> combinedSecrets;
                std::copy_n(finalcandlist[i].secret.extract_as_byte_array().begin(), 32, combinedSecrets.begin());
                std::copy_n(finalcandlist[i+1].secret.extract_as_byte_array().begin(), 32, combinedSecrets.begin() + 32);
                checksum256 result = sha256((char *) &combinedSecrets, sizeof(combinedSecrets));
                
                auto leastsignificantbyte = result.extract_as_byte_array().back();
                auto leastsignificantbit = ((int)leastsignificantbyte & 1);

                if(leastsignificantbit) finalcandlist.erase(finalcandlist.begin()+(i+1));
                else finalcandlist.erase(finalcandlist.begin()+i);
                
                if((finalcandlist.size() + producersinpool) <= info.requirednumberofproducers) return finalcandlist;
            }
        }
        return finalcandlist;
    }

    void blockbase::insertcandidate(eosio::name owner, eosio::name candidate, uint64_t &worktimeinseconds, std::string &publickey, checksum256 secrethash) {
        candidatesIndex _candidates(_self, owner.value);
        _candidates.emplace(candidate, [&](auto &candidateI) {
            candidateI.key = candidate;
            candidateI.worktimeinseconds = worktimeinseconds;
            candidateI.publickey = publickey;
            candidateI.secrethash = secrethash;
        });
    }

    void blockbase::addprod(eosio::name owner, blockbase::candidates candidate) {
        producersIndex _producers(_self, owner.value);
        _producers.emplace(owner, [&](auto &producer) {
            producer.key = candidate.key;
            producer.publickey = candidate.publickey;
            producer.worktimeinseconds = candidate.worktimeinseconds;
            producer.warning = WARNING_CLEAR;
            producer.isready = false;
            producer.startinsidechaindate = eosio::current_block_time().to_time_point().sec_since_epoch();
        });
    }
    
    void blockbase::addpublickey(eosio::name owner, eosio::name producer, std::string publickey) {
        ipsIndex _ips(_self, owner.value);
        _ips.emplace(owner, [&](auto &ipsI) {
            ipsI.key = producer;
            ipsI.publickey = publickey;
        });
    }

    void blockbase::infomanage(eosio::name owner, blockbase::contractinfo infojson) {
        infoIndex _infos(_self, owner.value);

        auto info = _infos.find(owner.value);
        if(info != _infos.end()) _infos.erase(info);
         
        _infos.emplace(owner, [&](auto &infoI) {
               infoI.key = owner;
            infoI.paymentperblock = infojson.paymentperblock;
            infoI.minimumcandidatestake = infojson.minimumcandidatestake;
            infoI.requirednumberofproducers = infojson.requirednumberofproducers;
            infoI.candidaturetime = infojson.candidaturetime;
            infoI.sendsecrettime = infojson.sendsecrettime;
            infoI.ipsendtime = infojson.ipsendtime;
            infoI.ipreceivetime = infojson.ipreceivetime;
            infoI.candidatureenddate = 0;
            infoI.secretenddate = 0;
            infoI.ipsendenddate = 0;
            infoI.ipreceiveenddate = 0;
            infoI.blocktimeduration = infojson.blocktimeduration;
            infoI.blocksbetweensettlement = infojson.blocksbetweensettlement;
            infoI.sizeofblockinbytes = infojson.sizeofblockinbytes;
        });
    }

    void blockbase::setenddate(eosio::name owner, uint8_t type) {
        infoIndex _infos(_self, owner.value);
        auto info = _infos.find(owner.value);
        _infos.modify(info, owner, [&](auto &infoI) {
            if(type == CANDIDATURE_TIME_ID) infoI.candidatureenddate = (info->candidaturetime) + eosio::current_block_time().to_time_point().sec_since_epoch();
            if(type == SEND_TIME_ID) infoI.ipsendenddate = (info->ipsendtime) + eosio::current_block_time().to_time_point().sec_since_epoch();
            if(type == SECRET_TIME_ID) infoI.secretenddate = (info->sendsecrettime) + eosio::current_block_time().to_time_point().sec_since_epoch();
            if(type == RECEIVE_TIME_ID) infoI.ipreceiveenddate = (info->ipreceivetime) + eosio::current_block_time().to_time_point().sec_since_epoch();
        });
    }

    void blockbase::changestate(struct blockbase::contractst states){
        stateIndex _states(_self, states.key.value);
        auto state = _states.find(states.key.value);
        if(state == _states.end()) {
            _states.emplace(states.key, [&](auto &stateI) {
                stateI.key = states.key;
                stateI.startchain = states.startchain;
                stateI.configtime = states.configtime;
                stateI.candidaturetime = states.candidaturetime;
                stateI.secrettime = states.secrettime;
                stateI.ipsendtime = states.ipsendtime;
                stateI.ipreceivetime = states.ipreceivetime;
                stateI.productiontime = states.productiontime;
            });
        } else {
            _states.modify(state, states.key, [&](auto &stateI) {
                if(states.startchain != stateI.startchain) stateI.startchain = states.startchain;
                if(states.configtime != stateI.configtime) stateI.configtime = states.configtime;
                if(states.candidaturetime != stateI.candidaturetime) stateI.candidaturetime = states.candidaturetime;
                if(states.secrettime != stateI.secrettime) stateI.secrettime = states.secrettime;
                if(states.ipsendtime != stateI.ipsendtime) stateI.ipsendtime = states.ipsendtime;
                if(states.ipreceivetime != stateI.ipreceivetime) stateI.ipreceivetime = states.ipreceivetime;
                if(states.productiontime != stateI.productiontime) stateI.productiontime = states.productiontime;
            });
        }
    }
    
#pragma endregion
