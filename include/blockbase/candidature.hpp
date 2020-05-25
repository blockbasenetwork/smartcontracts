#pragma region Validation Methods
    

    bool blockbase::IsSecretValid(eosio::name owner, eosio::name name, checksum256 secret) {
        candidatesIndex _candidates(_self, owner.value);
        auto candidate = _candidates.find(name.value);
        if(sizeof(secret) <= 0) return false;
        auto hash = candidate -> secret_hash;
        auto secretArray = secret.extract_as_byte_array();
        checksum256 result = sha256((char *) &secretArray, sizeof(secretArray));
        return result == hash;
    }

    bool blockbase::IsPublicKeyValid(std::string publicKey){	
        return publicKey.size() == 53 && publicKey.substr(0,3) == "EOS";
    }

    bool blockbase::IsConfigurationValid(blockbase::contractinfo info) {
        auto numberOfProducersRequired = info.number_of_validator_producers_required + info.number_of_history_producers_required + info.number_of_full_producers_required;
        if(info.candidature_phase_duration_in_seconds < MIN_CANDIDATURE_TIME_IN_SECONDS) return false;
        if(info.ip_sending_phase_duration_in_seconds < MIN_IP_SEND_TIME_IN_SECONDS) return false; 
        if(info.ip_retrieval_phase_duration_in_seconds < MIN_IP_SEND_TIME_IN_SECONDS) return false;
        if(numberOfProducersRequired < MIN_REQUIRED_PRODUCERS) return false;
        return info.min_candidature_stake >= MIN_CANDIDATE_STAKE;
    }

    bool blockbase::IsCandidaturePhase(eosio::name owner) {
        stateIndex _states(_self, owner.value);
        auto state = _states.find(owner.value);
        return ((state -> is_configuration_phase == true && state -> is_production_phase == false) || (state -> is_configuration_phase == true && state -> is_production_phase == true)) && state -> has_chain_started == true && state != _states.end();
    }

    bool blockbase::IsCandidateValid(eosio::name owner, eosio::name producer) {
        producersIndex _producers(_self, owner.value);
        blacklistIndex _blacklists(_self, owner.value);
        candidatesIndex _candidates(_self, owner.value);
        return (_producers.find(producer.value) == _producers.end()) && (_candidates.find(producer.value) == _candidates.end()) && (_blacklists.find(producer.value) == _blacklists.end());
    }

    uint8_t blockbase::CalculateNumberOfIPsRequired(float numberOfProducers) {
        if(numberOfProducers == 1) return 0;
        return ceil(numberOfProducers/4.0);
    }

    std::vector<struct blockbase::candidates> blockbase::RunCandidatesSelection(eosio::name owner) {
        std::vector<struct blockbase::candidates> candidateSelection;
        for (int i = 1; i < 4; i++) {
            auto candidatesToAdd = RunCandidatesSelectionForType(owner, i);
            candidateSelection.insert(candidateSelection.end(), candidatesToAdd.begin(), candidatesToAdd.end());
        }
        return candidateSelection;
    }

    std::vector<struct blockbase::candidates> blockbase::RunCandidatesSelectionForType(eosio::name owner, uint8_t producerType) {
        infoIndex _infos(_self, owner.value);
        candidatesIndex _candidates(_self, owner.value);
        producersIndex _producers(_self, owner.value);
        auto info = _infos.get(owner.value);
        int32_t numberOfProducersRequired = producerType == 1 ? info.number_of_validator_producers_required : producerType == 2 ? info.number_of_history_producers_required : info.number_of_full_producers_required;
        
        std::vector<struct blockbase::candidates> selectedCandidateList;
        std::vector<struct blockbase::producers> producersOfSelectedType;

        if (numberOfProducersRequired == 0) return selectedCandidateList;

        for(auto candidate : _candidates){
            if(candidate.producer_type == producerType && IsSecretValid(owner, candidate.key, candidate.secret)) selectedCandidateList.push_back(candidate);
        }
        for(auto producer : _producers){
            if(producer.producer_type == producerType) producersOfSelectedType.push_back(producer);
        }

        if (selectedCandidateList.size() == 0) return selectedCandidateList;
        
        struct StakeComparator{
            explicit StakeComparator(eosio::name sidechain_) : sidechain(sidechain_) {}

            bool operator()(blockbase::candidates cand1, blockbase::candidates cand2) const{
                eosio::asset cstake1 = blockbasetoken::get_stake(BLOCKBASE_TOKEN, sidechain, cand1.key);
                eosio::asset cstake2 = blockbasetoken::get_stake(BLOCKBASE_TOKEN, sidechain, cand2.key);
                return cstake1 < cstake2;
            }
            eosio::name sidechain;
        };

        std::sort(selectedCandidateList.begin(), selectedCandidateList.end(), StakeComparator(_self));

        while((selectedCandidateList.size() + producersOfSelectedType.size()) > numberOfProducersRequired){
            for(auto i = 0; i < selectedCandidateList.size(); i += 2){
                std::array<uint8_t, 64> combinedSecrets;
                std::copy_n(selectedCandidateList[i].secret.extract_as_byte_array().begin(), 32, combinedSecrets.begin());
                std::copy_n(selectedCandidateList[i+1].secret.extract_as_byte_array().begin(), 32, combinedSecrets.begin() + 32);
                checksum256 result = sha256((char *) &combinedSecrets, sizeof(combinedSecrets));
                
                auto leastsignificantbyte = result.extract_as_byte_array().back();
                auto leastsignificantbit = ((int)leastsignificantbyte & 1);

                if(leastsignificantbit) selectedCandidateList.erase(selectedCandidateList.begin()+(i+1));
                else selectedCandidateList.erase(selectedCandidateList.begin()+i);
                
                if((selectedCandidateList.size() + producersOfSelectedType.size()) <= numberOfProducersRequired) return selectedCandidateList;
            }
        }
        return selectedCandidateList;
    }

    void blockbase::AddCandidatesWithReservedSeat(eosio::name owner) {
        candidatesIndex _candidates(_self, owner.value);
        reservedseatIndex _reservedseats(_self, owner.value);
        for (auto candidate : _candidates) {
            auto reservedSeat = _reservedseats.find(candidate.key.value);
            if (reservedSeat != _reservedseats.end()) {
                AddProducerDAM(owner, candidate);
                AddPublicKeyDAM(owner, candidate.key, candidate.public_key);
                RemoveCandidateDAM(owner, candidate.key);
            }   
        }
    }

    void blockbase::AddCandidateDAM(eosio::name owner, eosio::name candidate, std::string &publicKey, checksum256 secretHash, uint8_t producerType) {
        candidatesIndex _candidates(_self, owner.value);
        _candidates.emplace(candidate, [&](auto &newCandidateI) {
            newCandidateI.key = candidate;
            newCandidateI.work_duration_in_seconds = std::numeric_limits<int64_t>::max();
            newCandidateI.public_key = publicKey;
            newCandidateI.secret_hash = secretHash;
            newCandidateI.producer_type = producerType;
        });
    }

    void blockbase::AddProducerDAM(eosio::name owner, blockbase::candidates candidate) {
        producersIndex _producers(_self, owner.value);
        _producers.emplace(owner, [&](auto &newProducerI) {
            newProducerI.key = candidate.key;
            newProducerI.public_key = candidate.public_key;
            newProducerI.work_duration_in_seconds = candidate.work_duration_in_seconds;
            newProducerI.warning_type = WARNING_TYPE_CLEAR;
            newProducerI.is_ready_to_produce = false;
            newProducerI.sidechain_start_date_in_seconds = eosio::current_block_time().to_time_point().sec_since_epoch();
            newProducerI.producer_type = candidate.producer_type;
        });
    }
    
    void blockbase::AddPublicKeyDAM(eosio::name owner, eosio::name producer, std::string publicKey) {
        ipsIndex _ips(_self, owner.value);
        _ips.emplace(owner, [&](auto &ipsI) {
            ipsI.key = producer;
            ipsI.public_key = publicKey;
        });
    }

    void blockbase::UpdateContractInfoDAM(eosio::name owner, blockbase::contractinfo informationJson) {
        infoIndex _infos(_self, owner.value);

        auto info = _infos.find(owner.value);
        if(info != _infos.end()) _infos.erase(info);
         
        _infos.emplace(owner, [&](auto &newInfoI) {
            newInfoI.key = owner;
            newInfoI.max_payment_per_block_validator_producers = informationJson.max_payment_per_block_validator_producers;
            newInfoI.max_payment_per_block_history_producers = informationJson.max_payment_per_block_history_producers;
            newInfoI.max_payment_per_block_full_producers = informationJson.max_payment_per_block_full_producers;
            newInfoI.min_payment_per_block_validator_producers = informationJson.min_payment_per_block_validator_producers;
            newInfoI.min_payment_per_block_history_producers = informationJson.min_payment_per_block_history_producers;
            newInfoI.min_payment_per_block_full_producers = informationJson.min_payment_per_block_full_producers;
            newInfoI.min_candidature_stake = informationJson.min_candidature_stake;
            newInfoI.number_of_validator_producers_required = informationJson.number_of_validator_producers_required;
            newInfoI.number_of_history_producers_required = informationJson.number_of_history_producers_required;
            newInfoI.number_of_full_producers_required = informationJson.number_of_full_producers_required;
            newInfoI.candidature_phase_duration_in_seconds = informationJson.candidature_phase_duration_in_seconds;
            newInfoI.secret_sending_phase_duration_in_seconds = informationJson.secret_sending_phase_duration_in_seconds;
            newInfoI.ip_sending_phase_duration_in_seconds = informationJson.ip_sending_phase_duration_in_seconds;
            newInfoI.ip_retrieval_phase_duration_in_seconds = informationJson.ip_retrieval_phase_duration_in_seconds;
            newInfoI.candidature_phase_end_date_in_seconds = 0;
            newInfoI.secret_sending_phase_end_date_in_seconds = 0;
            newInfoI.ip_sending_phase_end_date_in_seconds = 0;
            newInfoI.ip_retrieval_phase_end_date_in_seconds = 0;
            newInfoI.block_time_in_seconds = informationJson.block_time_in_seconds;
            newInfoI.num_blocks_between_settlements = informationJson.num_blocks_between_settlements;
            newInfoI.block_size_in_bytes = informationJson.block_size_in_bytes;
        });
    }

    void blockbase::SetEndDateDAM(eosio::name owner, uint8_t type) {
        infoIndex _infos(_self, owner.value);
        auto info = _infos.find(owner.value);
        _infos.modify(info, owner, [&](auto &infoI) {
            if(type == CANDIDATURE_TIME_ID) infoI.candidature_phase_end_date_in_seconds = (info->candidature_phase_duration_in_seconds) + eosio::current_block_time().to_time_point().sec_since_epoch();
            if(type == SEND_TIME_ID) infoI.ip_sending_phase_end_date_in_seconds = (info->ip_sending_phase_duration_in_seconds) + eosio::current_block_time().to_time_point().sec_since_epoch();
            if(type == SECRET_TIME_ID) infoI.secret_sending_phase_end_date_in_seconds = (info->secret_sending_phase_duration_in_seconds) + eosio::current_block_time().to_time_point().sec_since_epoch();
            if(type == RECEIVE_TIME_ID) infoI.ip_retrieval_phase_end_date_in_seconds = (info->ip_retrieval_phase_duration_in_seconds) + eosio::current_block_time().to_time_point().sec_since_epoch();
        });
    }

    void blockbase::ChangeContractStateDAM(struct blockbase::contractst states){
        stateIndex _states(_self, states.key.value);
        auto state = _states.find(states.key.value);
        if(state == _states.end()) {
            _states.emplace(states.key, [&](auto &newStateI) {
                newStateI.key = states.key;
                newStateI.has_chain_started = states.has_chain_started;
                newStateI.is_configuration_phase = states.is_configuration_phase;
                newStateI.is_candidature_phase = states.is_candidature_phase;
                newStateI.is_secret_sending_phase = states.is_secret_sending_phase;
                newStateI.is_ip_sending_phase = states.is_ip_sending_phase;
                newStateI.is_ip_retrieving_phase = states.is_ip_retrieving_phase;
                newStateI.is_production_phase = states.is_production_phase;
            });
        } else {
            _states.modify(state, states.key, [&](auto &stateI) {
                if(states.has_chain_started != stateI.has_chain_started) stateI.has_chain_started = states.has_chain_started;
                if(states.is_configuration_phase != stateI.is_configuration_phase) stateI.is_configuration_phase = states.is_configuration_phase;
                if(states.is_candidature_phase != stateI.is_candidature_phase) stateI.is_candidature_phase = states.is_candidature_phase;
                if(states.is_secret_sending_phase != stateI.is_secret_sending_phase) stateI.is_secret_sending_phase = states.is_secret_sending_phase;
                if(states.is_ip_sending_phase != stateI.is_ip_sending_phase) stateI.is_ip_sending_phase = states.is_ip_sending_phase;
                if(states.is_ip_retrieving_phase != stateI.is_ip_retrieving_phase) stateI.is_ip_retrieving_phase = states.is_ip_retrieving_phase;
                if(states.is_production_phase != stateI.is_production_phase) stateI.is_production_phase = states.is_production_phase;
            });
        }
    }
    
    void blockbase::RemoveCandidateDAM(eosio::name owner, eosio::name candidate) {
        candidatesIndex _candidates(_self, owner.value);
        auto candidateInSidechainToRemove = _candidates.find(candidate.value);
        _candidates.erase(candidateInSidechainToRemove);

        eosio::print("Candidate removed. \n");  
    }
#pragma endregion
