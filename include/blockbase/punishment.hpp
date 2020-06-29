void blockbase::EvaluateProducer(eosio::name owner, eosio::name producer, uint16_t failedBlocks, uint16_t producedBlocks) {
    warningsIndex _warnings(_self, owner.value);
    auto producerWarningId = GetSpecificProducerWarningId(owner, producer, WARNING_TYPE_BLOCKS_FAILED);
    uint16_t totalBlocks = producedBlocks + failedBlocks;
    uint16_t totalFailedBlocksPermited = ceil(MIN_BLOCKS_THRESHOLD_FOR_PUNISH * totalBlocks);
    if(producerWarningId == -1 && failedBlocks >= totalFailedBlocksPermited) 
        AddWarningDAM(owner, producer, WARNING_TYPE_BLOCKS_FAILED);
    else if (producerWarningId != -1 && failedBlocks == 0) 
       ClearWarningDAM(owner, producer, producerWarningId);
}

void blockbase::CheckHistoryValidation(eosio::name owner) {
    histvalIndex _histval(_self, owner.value);
    warningsIndex _warnings(_self, owner.value);
    auto histval = _histval.begin();
    while (histval != _histval.end()) {
        auto producerWarningId =  GetSpecificProducerWarningId(owner, histval->key, WARNING_TYPE_HISTORY_VALIDATION_FAILED);
        if(producerWarningId == -1) {
            AddWarningDAM(owner, histval->key, WARNING_TYPE_HISTORY_VALIDATION_FAILED);
        }
        _histval.erase(histval);
    }
}

std::vector<struct blockbase::producers> blockbase::GetPunishedProducers(eosio::name owner) {
    producersIndex _producers(_self, owner.value);
    warningsIndex _warnings (_self, owner.value);
    std::vector<struct blockbase::producers> producerToPunish;
    for(auto& producer : _producers) {
        auto producerWarningId = GetSpecificProducerWarningId(owner, producer.key, WARNING_TYPE_PUNISH);
        if(producerWarningId != -1) producerToPunish.push_back(producer);
    }
    return producerToPunish;
}

std::vector<struct blockbase::producers> blockbase::GetProducersWhoFailedToSendIPs(eosio::name owner) {
    infoIndex _infos(_self, owner.value);
    ipsIndex _ips(_self, owner.value);
    producersIndex _producers(_self, owner.value);

    auto info = _infos.find(owner.value);
    auto numberOfProducersInChain = std::distance(_producers.begin(), _producers.end());
    
    uint8_t numberOfRequiredIPs = numberOfProducersInChain == 1 ? 0 : ceil(numberOfProducersInChain/4.0);
        
    std::vector<struct blockbase::producers> producersToRemove;
    for (auto producer : _producers) {
        auto ip = _ips.find(producer.key.value); 
        //TODO can't the encrypted IPs be bigger than the numberOfRequiredIPs? Is the producer also removed then?
        if (ip -> encrypted_ips.size() != numberOfRequiredIPs + 1) {
            producersToRemove.push_back(producer);
            continue;
        }
        for (auto encryptedip : ip -> encrypted_ips) {
            if (encryptedip == "") { //TODO: Check ip size
                producersToRemove.push_back(producer);
                break;
            }
        }
    }
    return producersToRemove;
}

    void blockbase::AddWarningDAM(eosio::name owner, eosio::name producer, uint8_t warningType) {
        warningsIndex _wannings(_self, owner.value);
        _wannings.emplace(owner, [&](auto &newProducerWarningI) {
            newProducerWarningI.key = _wannings.available_primary_key();
            newProducerWarningI.producer = producer;
            newProducerWarningI.warning_type = warningType;
            newProducerWarningI.warning_creation_date_in_seconds = eosio::current_block_time().to_time_point().sec_since_epoch();
            newProducerWarningI.producer_exit_date_in_seconds = 0;
        });
    }

void blockbase::ClearWarningDAM(eosio::name owner, eosio::name producer, uint64_t warningId) {
    warningsIndex _warnings(_self, owner.value);
    auto producerWarning = _warnings.find(warningId);
    _warnings.erase(producerWarning);
}

void blockbase::RemoveProducersDAM(eosio::name owner, std::vector<struct blockbase::producers> producers) {
    producersIndex _producers(_self, owner.value);
    auto itr = producers.begin();
    while (itr != producers.end()) {	
        auto producerToRemove = _producers.find(itr -> key.value);	
        _producers.erase(producerToRemove);
        itr = producers.erase(itr);
                    
        eosio::print("  Producer ", producerToRemove -> key, " was removed. \n");
    }
}

void blockbase::RemoveIPsDAM(eosio::name owner, std::vector<struct blockbase::producers> producers) {
    ipsIndex _ips(_self, owner.value);
    for(auto producer : producers) _ips.erase(_ips.find(producer.key.value));
    eosio::print("  Ips addresses removed. \n");
}

void blockbase::RemoveProducersDAM(eosio::name owner) {
    producersIndex _producers(_self, owner.value);
    currentprodIndex _currentprods(_self, owner.value);
    auto itr = _producers.begin();
    while (itr != _producers.end()) {
        auto currentProducer = _currentprods.find(itr -> key.value);
        if((_currentprods.end()) != currentProducer) _currentprods.erase(currentProducer);
        itr = _producers.erase(itr);		
    }
    eosio::print("  Producers were removed. \n");
}

void blockbase::RemoveIPsDAM(eosio::name owner) {
    producersIndex _producers(_self, owner.value);
    ipsIndex _ips(_self, owner.value);
    if (std::distance(_producers.begin(), _producers.end()) > 0 || std::distance(_ips.begin(), _ips.end()) > 0) {
        auto ipAddress = _ips.begin();
        while (ipAddress != _ips.end()) ipAddress = _ips.erase(ipAddress);
    }
}

void blockbase::RemoveAllProducerWarningsDAM(eosio::name owner, std::vector<struct producers> producers) {
    warningsIndex _warnings(_self, owner.value);
    std::map<uint64_t, eosio::name> warningsToClearProducerIdList;
    for (auto producer : producers) {
        for(auto warning : _warnings) {
            if(warning.producer == producer.key) {
                warningsToClearProducerIdList.insert(std::pair<uint64_t, eosio::name>(warning.key, producer.key));
            }
        }
    }

    for(auto const& warningToClearProducerId : warningsToClearProducerIdList) {
        ClearWarningDAM(owner, warningToClearProducerId.second, warningToClearProducerId.first);
    }
}

