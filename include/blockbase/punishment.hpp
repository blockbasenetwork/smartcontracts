
void blockbase::punishprod(eosio::name owner) {
    action(
        permission_level{owner, eosio::name("active")}, 
        BLOCKBASE_TOKEN, eosio::name("prodpunish"), 
        std::make_tuple(owner, _self)
    ).send();
}

void blockbase::changewarning(eosio::name owner, eosio::name producer, uint16_t failedblocks, uint16_t producedblocks) {
    producersIndex _producers(_self, owner.value);
    blacklistIndex _blacklists(_self, owner.value);
    auto producerI = _producers.find(producer.value);
    uint16_t totalblocks = producedblocks + failedblocks;
    uint16_t totalfailedblockspermited = floor(THRESHOLD_FOR_PUNISH * totalblocks);
    if (failedblocks > 0 && failedblocks < totalfailedblockspermited) {
        if (producerI -> warning == WARNING_FLAGGED) {
            updatewarning(owner, producer, WARNING_PUNISH);

            action(
                permission_level{owner, eosio::name("active")}, 
                _self, eosio::name("blacklistprod"), 
                std::make_tuple(owner, producer)
            ).send();

        } else updatewarning(owner, producer, WARNING_FLAGGED);
    } else if (failedblocks > totalfailedblockspermited && failedblocks <= totalblocks) {
        updatewarning(owner, producer, WARNING_PUNISH);
        
        action(
            permission_level{owner, eosio::name("active")}, 
            _self, eosio::name("blacklistprod"), 
            std::make_tuple(owner, producer)
        ).send();

    } else if (failedblocks == 0 && totalblocks == producedblocks && producerI -> warning == WARNING_FLAGGED) updatewarning(owner, producer, WARNING_CLEAR);
}

std::vector<struct blockbase::producers> blockbase::checkbadprods(eosio::name owner) {
    producersIndex _producers(_self, owner.value);
    std::vector<struct blockbase::producers> producerstab;
    for (auto& producer : _producers) if(producer.worktimeinseconds + producer.startinsidechaindate <= eosio::current_block_time().to_time_point().sec_since_epoch() || producer.warning == WARNING_PUNISH) producerstab.push_back(producer);
    return producerstab;
}

std::vector<struct blockbase::producers> blockbase::checksendprods(eosio::name owner) {
    infoIndex _infos(_self, owner.value);
    ipsIndex _ips(_self, owner.value);
    producersIndex _producers(_self, owner.value);

    auto info = _infos.find(owner.value);
    uint8_t producersneeded = numberofips(info -> requirednumberofproducers);
        
    std::vector<struct blockbase::producers> finalproducerlist;
    for (auto producer : _producers) {
        auto ip = _ips.find(producer.key.value);
        if (ip -> encryptedips.size() != producersneeded + 1) {
            finalproducerlist.push_back(producer);
            continue;
        }
        for (auto encryptedip : ip -> encryptedips) {
            if (encryptedip == "") { //TODO: Check ip size
                finalproducerlist.push_back(producer);
                continue;
            }
        }
    }
    return finalproducerlist;
}

void blockbase::updatewarning(eosio::name owner, eosio::name producer, uint8_t warning) {
    producersIndex _producers(_self, owner.value);
    auto producerI = _producers.find(producer.value);
    _producers.modify(producerI, owner, [&](auto &producerIT) {
        producerIT.warning = warning;
    });
}

void blockbase::deleteprods(eosio::name owner, std::vector<struct blockbase::producers> producers) {
    producersIndex _producers(_self, owner.value);
    auto itr = producers.begin();
    while (itr != producers.end()) {	
        auto producertoremove = _producers.find(itr -> key.value);	
        _producers.erase(producertoremove);
        itr = producers.erase(itr);
                    
        eosio::print("  Producer ", producertoremove -> key, " was removed. \n");
    }
}

void blockbase::deleteips(eosio::name owner, std::vector<struct blockbase::producers> producers) {
    ipsIndex _ips(_self, owner.value);
    for(auto producer : producers) _ips.erase(_ips.find(producer.key.value));
    eosio::print("  Ips addresses removed. \n");
}

void blockbase::deleteprods(eosio::name owner) {
    producersIndex _producers(_self, owner.value);
    currentprodIndex _currentprods(_self, owner.value);
    auto itr = _producers.begin();
    while (itr != _producers.end()) {
        auto cproducer = _currentprods.find(itr -> key.value);
        if((_currentprods.end()) != cproducer) _currentprods.erase(cproducer);
        itr = _producers.erase(itr);		
    }
    eosio::print("  Producers were removed. \n");
}

void blockbase::deleteips(eosio::name owner) {
    producersIndex _producers(_self, owner.value);
    ipsIndex _ips(_self, owner.value);
    if (std::distance(_producers.begin(), _producers.end()) > 0 || std::distance(_ips.begin(), _ips.end()) > 0) {
        auto ipaddress = _ips.begin();
        while (ipaddress != _ips.end()) ipaddress = _ips.erase(ipaddress);
    }
}

