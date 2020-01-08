#pragma region producing Methods
//TODO caso miner fique ready a meio do mining, impedir que seja expulso por nao minar
void blockbase::computation(eosio::name owner) {
    infoIndex _infos(_self, owner.value);
    blockscountIndex _blockscount(_self, owner.value);
    producersIndex _producers(_self, owner.value);
    uint8_t producedblocks = 0, failedblocks = 0;
    auto blockpayment = _infos.find(owner.value)->paymentperblock;
    std::vector<blockbase::producers> readyproducers = blockbase::getreadyprods(owner);
    bool toblacklist = false;
    for (struct blockbase::producers &producer : readyproducers) {
        auto producerblockstable = _blockscount.find(producer.key.value);
        if (producerblockstable != _blockscount.end()) {
            producedblocks = producerblockstable->blocksproduced;
            failedblocks = producerblockstable->blocksfailed;
        }
        changewarning(owner, producer.key, failedblocks, producedblocks);
        
        auto producerwarning = _producers.find(producer.key.value);
        if(producerwarning -> warning == WARNING_PUNISH) toblacklist = true;
        if(producerwarning -> warning != WARNING_PUNISH && producedblocks > 0) rewardprod(owner, producer.key, (producedblocks * (blockpayment)));
    }
    if(toblacklist) {       
        action(
            permission_level{owner, eosio::name("active")}, 
            _self, eosio::name("blacklistprod"), 
            std::make_tuple(owner)
        ).send();
    }
    startcount(owner, true);
    enoughclientstake(owner);
    eosio::print("Computation has ended. \n");
}
//TODO States not used in this method - to remove.
void blockbase::manageprod(eosio::name owner) {
    infoIndex _infos(_self, owner.value);
    auto numberofproducersrequired = _infos.find(owner.value)->requirednumberofproducers;
    std::vector<struct producers> producerstoremove = checkbadprods(owner);
    if(producerstoremove.size() > 0){
        deleteips(owner, producerstoremove);
        deleteprods(owner, producerstoremove);
        deletecprod(owner,producerstoremove);
    }
    eraseblockcount(owner);
    startcount(owner);
}

void blockbase::enoughclientstake(eosio::name owner) {
    infoIndex _infos(_self, owner.value);
    auto info = _infos.find(owner.value);

    asset clientstake = blockbasetoken::get_stake(BLOCKBASE_TOKEN, owner, owner);

    if (clientstake.amount < ((info->paymentperblock) * (info->blocksbetweensettlement))) {
        changestate({owner, true, false, false, false, false, false, false});
        deleteblockcount(owner);
        deleteips(owner);
        deleteprods(owner);
        eosio::print("  No stake left for next payment, so all producers left the chain without penaltis! \n");
        eosio::print("  Configure the chain again. \n");
    }
}

void blockbase::checkprodstake(eosio::name owner) {
    producersIndex _producers(_self, owner.value);
    for (auto producer : _producers) {
        asset prodstake = blockbasetoken::get_stake(BLOCKBASE_TOKEN, owner, producer.key);

        if (prodstake.amount <= 0){
            action(
                permission_level{_self, eosio::name("active")}, 
                BLOCKBASE_TOKEN, eosio::name("leaveledger"), 
                std::make_tuple(owner, producer, owner)
            ).send();
        }
        eosio::print("  Producer ", producer.key," removed from stakeledger. \n");
    }
}

void blockbase::blockcount(eosio::name owner, eosio::name producer) {
    blockheadersIndex _blockheaders(_self, owner.value);
    infoIndex _infos(_self, owner.value);
    blockscountIndex _blockscount(_self, owner.value);

    auto info = _infos.find(owner.value);
    auto producerblockcount = _blockscount.find(producer.value);
    if (std::distance(_blockheaders.begin(), _blockheaders.end()) > 0) {
        if (((--_blockheaders.end())->timestamp) >= (eosio::current_block_time().to_time_point().sec_since_epoch() - ((info->blocktimeduration) + (info->blocktimeduration)/2)) && producer == eosio::name((--_blockheaders.end())->producer) && (--_blockheaders.end())->isverified == true) {
            _blockscount.modify(producerblockcount, owner, [&](auto &blockcountI) {
                blockcountI.blocksproduced += 1;
            });
            return;
        }
    }
    _blockscount.modify(producerblockcount, owner, [&](auto &blockcountI) {
        blockcountI.blocksfailed += 1;
    });
    if (std::distance(_blockheaders.begin(), _blockheaders.end()) > 0) {
        auto blockheader = _blockheaders.find((--_blockheaders.end())->sequencenumber);
        if (blockheader->isverified == false) {
            _blockheaders.erase(blockheader);
            eosio::print("  Block was not validated by the production pool, so is going to be deleted. \n");
        }
    }
}

blockbase::producers blockbase::getnextprod(eosio::name owner) {
    currentprodIndex _currentprods(_self, owner.value);
    producersIndex _producers(_self, owner.value);
      std::vector<blockbase::producers> readyproducers = blockbase::getreadyprods(owner);
    auto currentproducer = _currentprods.begin();
    struct blockbase::producers nextproducer;
    struct blockbase::producers firstproducer = _producers.get((readyproducers.front()).key.value);
    struct blockbase::producers lastproducer = _producers.get((readyproducers.back()).key.value);
    bool currentproducerfound = false;
    
    if (readyproducers.size() > 1 && currentproducer != _currentprods.end()) {
        for (struct blockbase::producers producer : readyproducers) {
            if (producer.key == currentproducer->producer) {
                if (producer.key == lastproducer.key) {
                    nextproducer = firstproducer;
                    break;
                } else if (producer.key != lastproducer.key) {
                    currentproducerfound = true;
                }
            } else if (currentproducerfound) {
                nextproducer = producer;
                break;
            }
        }
        return nextproducer;
    }
    return firstproducer;
}

void blockbase::nextcurrentprod(eosio::name owner, eosio::name nextproducer) {
    currentprodIndex _currentprods(_self, owner.value);
    auto currentproducer = _currentprods.find(CKEY.value);
    if (currentproducer != _currentprods.end()) {
        _currentprods.modify(currentproducer, owner, [&](auto &cproducer) {
            cproducer.producer = nextproducer;
            cproducer.startproductiontime = eosio::current_block_time().to_time_point().sec_since_epoch();
            cproducer.isblockproduced = false;
        });
    } else {
        _currentprods.emplace(owner, [&](auto &cproducer) {
            cproducer.id = CKEY;
            cproducer.producer = nextproducer;
            cproducer.startproductiontime = eosio::current_block_time().to_time_point().sec_since_epoch();
            cproducer.isblockproduced = false;
        });
    }
}

void blockbase::deleteblockcount(eosio::name owner) {
    blockscountIndex _blockscount(_self, owner.value);
    if (std::distance(_blockscount.begin(), _blockscount.end()) != 0) {
        auto itr = _blockscount.begin();
        while (itr != _blockscount.end()) itr = _blockscount.erase(itr);	
    }
}

uint8_t blockbase::thresholdcalc(uint8_t producersnumber){
    return ceil(producersnumber / 2) + 1;
}

void blockbase::deletecprod(eosio::name owner, std::vector<struct producers> producerstoremove) {
    currentprodIndex _currentprods(_self, owner.value);
    auto currentproducer = _currentprods.find(CKEY.value);
    for(auto producers: producerstoremove) {
        if(producers.key == currentproducer -> producer) {
            _currentprods.erase(currentproducer);
        }
    }
}
#pragma endregion
