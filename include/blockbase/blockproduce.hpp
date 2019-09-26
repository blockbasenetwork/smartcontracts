bool blockbase::isblockprod(eosio::name owner, eosio::name producer){
    currentprodIndex _currentprods(_self, owner.value);
    auto currentproducer = _currentprods.find(CKEY.value);
    return currentproducer != _currentprods.end() && currentproducer -> isblockproduced == false;
}

bool blockbase::isprodtime(eosio::name owner, eosio::name producer) {
    currentprodIndex _currentprods(_self, owner.value);
    infoIndex _infos (_self, owner.value);
    producersIndex _producers(_self, owner.value);
    blockheadersIndex _blockheaders(_self, owner.value);

    auto currentproducer = _currentprods.find(CKEY.value);
    auto info = _infos.find(owner.value);
    auto producerI = _producers.find(producer.value);

    if (currentproducer == _currentprods.end() || producerI == _producers.end() || currentproducer -> producer != producer) return false;
    std::vector<blockbase::producers> readyproducers = blockbase::getreadyprods(owner);
    if (std::distance(_blockheaders.begin(), _blockheaders.end()) > 0 && readyproducers.size() >= 2 ) {	   
        if ((--_blockheaders.end()) -> producer == producerI -> key.to_string() &&
            ((--_blockheaders.end()) -> timestamp + ((info -> blocktimeduration) * readyproducers.size())) > eosio::current_block_time().to_time_point().sec_since_epoch()) return false;
    }
    return true;
}

//TODO: Check timestamp validation
bool blockbase::isblockvalid(eosio::name owner, blockbase::blockheaders block) {
    infoIndex _infos (_self, owner.value);
    blockheadersIndex _blockheaders(_self, owner.value);
    uint64_t blockstablesize = std::distance(_blockheaders.begin(), _blockheaders.end());
    auto info = _infos.find(owner.value);

    std::vector<struct blockbase::blockheaders> lastblocklist = getlastblock(owner);

    if(block.timestamp < eosio::current_block_time().to_time_point().sec_since_epoch() - (3*info->blocktimeduration) || block.timestamp > eosio::current_block_time().to_time_point().sec_since_epoch()) return false;

    if(lastblocklist.size() == 0 && blockstablesize == 0 && block.sequencenumber == 1) return true;

    if(lastblocklist.size() > 0 && blockstablesize <= 0) {
        auto lastblock = lastblocklist.back();
        if(lastblock.sequencenumber + 1 == block.sequencenumber && lastblock.blockhash == block.previousblockhash) return true;
    }

    if(blockstablesize > 0){
        auto blockheader = --_blockheaders.end();
        if(blockheader -> sequencenumber + 1 == block.sequencenumber && blockheader -> blockhash == block.previousblockhash) return true;
    }
    return false;
}

std::vector<struct blockbase::blockheaders> blockbase::getlastblock(eosio::name owner) {
    blockheadersIndex _blockheaders(_self, owner.value);
    std::vector<struct blockbase::blockheaders> blocklist;
    for(auto block : _blockheaders) if(block.islastblock) blocklist.push_back(block);
    return blocklist;
}

std::vector<struct blockbase::producers> blockbase::getreadyprods(eosio::name owner) {
    producersIndex _producers(_self, owner.value);
    std::vector<struct blockbase::producers> producerslist;
    if(std::distance(_producers.begin(), _producers.end()) > 0){ 
        for(auto producer : _producers) if(producer.isready) producerslist.push_back(producer);
    }
    return producerslist;
}

void blockbase::insertblock(eosio::name owner, eosio::name producer, blockbase::blockheaders block) {
    blockheadersIndex _blockheaders(_self, owner.value);
    currentprodIndex _currentprods(_self, owner.value);
    infoIndex _infos(_self, owner.value);
    auto info = _infos.find(owner.value);
    auto currentproducer = _currentprods.find(CKEY.value);
    
    _blockheaders.emplace(_self, [&](auto &newblockI) {
        newblockI.producer = block.producer;
        newblockI.blockhash = block.blockhash;
        newblockI.previousblockhash = block.previousblockhash;
        newblockI.sequencenumber = block.sequencenumber;
        newblockI.timestamp = block.timestamp;
        newblockI.producersignature = block.producersignature;
        newblockI.merkletreeroothash = block.merkletreeroothash;
        newblockI.isverified = false;
        newblockI.islastblock = false;
    });

    if(std::distance(_blockheaders.begin(), _blockheaders.end()) > (info->blocksbetweensettlement)) _blockheaders.erase(_blockheaders.begin());
    
    _currentprods.modify(currentproducer, producer, [&](auto &cproducer) {
        cproducer.isblockproduced = true;
    });
    eosio::print("  Block valid and submitted. \n");
}

void blockbase::startcount(eosio::name owner, bool computation) {
    producersIndex _producers(_self, owner.value);
    blockscountIndex _blockscount(_self, owner.value);
    for(auto producer : _producers) {
        auto blockcountprod = _blockscount.find(producer.key.value);
        if(blockcountprod == _blockscount.end()){
            _blockscount.emplace(owner, [&](auto &blockcount) {
                blockcount.key = producer.key;
                blockcount.blocksfailed = 0;
                blockcount.blocksproduced = 0;
            });
        } else if (computation) {
            _blockscount.modify(blockcountprod, owner, [&](auto &blockcount) {
                blockcount.blocksfailed = 0;
                blockcount.blocksproduced = 0;
            });
        }
    }
}

void blockbase::decisionmark(eosio::name owner){
    stateIndex _states(_self, owner.value);
    producersIndex _producers(_self, owner.value);
    currentprodIndex _currentprods(_self, owner.value);
    infoIndex _infos(_self, owner.value);
    auto info = _infos.find(owner.value);
    auto state = _states.find(owner.value);
    int32_t numberofproducersrequired = info -> requirednumberofproducers;
    uint8_t producersize = std::distance(_producers.begin(), _producers.end());
    if (producersize < numberofproducersrequired) {
        if (producersize < ceil(numberofproducersrequired * PRODUCERS_IN_CHAIN_THRESHOLD)) {
            if(state -> candidaturetime == false && state -> secrettime == false && state -> ipsendtime == false && state -> ipreceivetime == false) {
                changestate({owner, true, false, true, false, false, false, false});
                eosio::print("  Number of producers on the pool is below threshold, mining stoped and candidature time starting... \n");
                cancel_deferred(owner.value + CANDIDATURE_TIME_ID);
                setenddate(owner, CANDIDATURE_TIME_ID);
                delayedtx(owner, eosio::name("secrettime"), info -> candidaturetime, CANDIDATURE_TIME_ID);
            }
        } else if (producersize >= ceil(numberofproducersrequired * PRODUCERS_IN_CHAIN_THRESHOLD) && producersize < numberofproducersrequired) {
            if(state -> candidaturetime == false && state -> secrettime == false && state -> ipsendtime == false && state -> ipreceivetime == false) {
                changestate({owner, true, false, true, false, false, false, true});
                eosio::print("  Starting candidature time but mining continues. \n");
                cancel_deferred(owner.value + CANDIDATURE_TIME_ID);
                setenddate(owner, CANDIDATURE_TIME_ID);
                delayedtx(owner, eosio::name("secrettime"), info -> candidaturetime, CANDIDATURE_TIME_ID);
            }
            eosio::print("  Candidature in progress... \n");
        }
    }
    eosio::print("  Producer changed. \n");  
    delayedtx(owner, eosio::name("changecprod"), info -> blocktimeduration, CHANGE_PRODUCER_ID);
}

void blockbase::eraseblockcount(eosio::name owner) {
    producersIndex _producers(_self, owner.value);
    blockscountIndex _blockscount(_self, owner.value);
    std::vector<eosio::name> bname;
    for(auto bcount : _blockscount) {
        auto producertemp = _producers.find(bcount.key.value);
        if(_producers.end() == producertemp) bname.push_back(bcount.key);
    }
    if(bname.size() != 0) for(auto producer : bname) _blockscount.erase(_blockscount.find(producer.value));
}