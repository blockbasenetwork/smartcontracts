void blockbase::EvaluateProducer(eosio::name owner, eosio::name producer, uint16_t failedBlocks, uint16_t producedBlocks) {
    producersIndex _producers(_self, owner.value);
    auto producerInTable = _producers.find(producer.value);
    uint16_t totalBlocks = producedBlocks + failedBlocks;
    uint16_t totalFailedBlocksPermited = ceil(MIN_BLOCKS_THRESHOLD_FOR_PUNISH * totalBlocks);
    if (failedBlocks >= totalFailedBlocksPermited) 
    {
        if (producerInTable -> warning_type == WARNING_TYPE_FLAGGED)
        {
            UpdateWarningDAM(owner, producer, WARNING_TYPE_PUNISH);
        }
        else
        {
            UpdateWarningDAM(owner, producer, WARNING_TYPE_FLAGGED);
        }
    } 
    else if (failedBlocks == 0 && producerInTable -> warning_type == WARNING_TYPE_FLAGGED) 
    {
        UpdateWarningDAM(owner, producer, WARNING_TYPE_CLEAR);
    }
}

std::vector<struct blockbase::producers> blockbase::GetPunishedProducers(eosio::name owner) {
    producersIndex _producers(_self, owner.value);
    std::vector<struct blockbase::producers> producerToPunish;
    for(auto& producer : _producers) {
        if(producer.warning_type == WARNING_TYPE_PUNISH) producerToPunish.push_back(producer);
    }
    return producerToPunish;
}

std::vector<struct blockbase::producers> blockbase::GetProducersWhoFailedToSendIPs(eosio::name owner) {
    infoIndex _infos(_self, owner.value);
    ipsIndex _ips(_self, owner.value);
    producersIndex _producers(_self, owner.value);

    auto info = _infos.find(owner.value);
    auto numberOfProducersRequired = info->number_of_validator_producers_required + info->number_of_history_producers_required + info->number_of_full_producers_required;
    uint8_t numberOfRequiredIPs = CalculateNumberOfIPsRequired(numberOfProducersRequired);
        
    std::vector<struct blockbase::producers> producersToRemove;
    for (auto producer : _producers) {
        auto ip = _ips.find(producer.key.value);
        if (ip -> encrypted_ips.size() != numberOfRequiredIPs + 1) {
            producersToRemove.push_back(producer);
            continue;
        }
        for (auto encryptedip : ip -> encrypted_ips) {
            if (encryptedip == "") { //TODO: Check ip size
                producersToRemove.push_back(producer);
                continue;
            }
        }
    }
    return producersToRemove;
}

void blockbase::UpdateWarningDAM(eosio::name owner, eosio::name producer, uint8_t warningType) {
    producersIndex _producers(_self, owner.value);
    auto producerInSidechain= _producers.find(producer.value);
    _producers.modify(producerInSidechain, owner, [&](auto &producerIT) {
        producerIT.warning_type = warningType;
    });
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

