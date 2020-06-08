bool blockbase::HasBlockBeenProduced(eosio::name owner, eosio::name producer){
    currentprodIndex _currentprods(_self, owner.value);
    auto currentProducer = _currentprods.find(CKEY.value);
    return currentProducer != _currentprods.end() && currentProducer -> has_produced_block == true;
}

//TODO Test this method
bool blockbase::IsProducerTurn(eosio::name owner, eosio::name producer) {
    currentprodIndex _currentprods(_self, owner.value);
    infoIndex _infos (_self, owner.value);
    producersIndex _producers(_self, owner.value);
    blockheadersIndex _blockheaders(_self, owner.value);

    auto currentProducer = _currentprods.find(CKEY.value);
    auto info = _infos.find(owner.value);
    auto producerInSidechain = _producers.find(producer.value);

    if (currentProducer == _currentprods.end() || producerInSidechain == _producers.end() || currentProducer -> producer != producer) return false;
    std::vector<blockbase::producers> readyProducersInSidechain = blockbase::GetReadyProducers(owner);
    if (std::distance(_blockheaders.begin(), _blockheaders.end()) > 0 && readyProducersInSidechain.size() >= 2 ) {	   
        if ((--_blockheaders.end()) -> producer == producerInSidechain -> key.to_string()) return false;
    }
    return true;
}

//TODO: Check timestamp validation
bool blockbase::IsBlockValid(eosio::name owner, blockbase::blockheaders block) {
    infoIndex _infos (_self, owner.value);
    blockheadersIndex _blockheaders(_self, owner.value);
    uint64_t blockHeadersTableSize = std::distance(_blockheaders.begin(), _blockheaders.end());
    auto info = _infos.find(owner.value);

    std::vector<struct blockbase::blockheaders> lastestBlockList = GetLatestBlock(owner);

    if(block.timestamp < eosio::current_block_time().to_time_point().sec_since_epoch() - (3*info->block_time_in_seconds) || block.timestamp > eosio::current_block_time().to_time_point().sec_since_epoch()) return false;
    if(block.block_size_in_bytes > info -> block_size_in_bytes) return false;
    if(lastestBlockList.size() == 0 && blockHeadersTableSize == 0 && block.sequence_number == 1) return true;

    if(lastestBlockList.size() > 0 && blockHeadersTableSize <= 0) {
        auto lastblock = lastestBlockList.back();
        if(lastblock.sequence_number + 1 == block.sequence_number && lastblock.block_hash == block.previous_block_hash) return true;
    }

    if(blockHeadersTableSize > 0){
        auto blockHeader = --_blockheaders.end();
        if(blockHeader -> sequence_number + 1 == block.sequence_number && blockHeader -> block_hash == block.previous_block_hash) return true;
    }
    return false;
}

std::vector<struct blockbase::blockheaders> blockbase::GetLatestBlock(eosio::name owner) {
    blockheadersIndex _blockheaders(_self, owner.value);
    std::vector<struct blockbase::blockheaders> blockHeaderListToReturn;
    for(auto block : _blockheaders) if(block.is_latest_block) blockHeaderListToReturn.push_back(block);
    return blockHeaderListToReturn;
}

std::vector<struct blockbase::producers> blockbase::GetReadyProducers(eosio::name owner) {
    producersIndex _producers(_self, owner.value);
    std::vector<struct blockbase::producers> readyProducersListToReturn;
    if(std::distance(_producers.begin(), _producers.end()) > 0){ 
        for(auto producer : _producers) if(producer.is_ready_to_produce) readyProducersListToReturn.push_back(producer);
    }
    return readyProducersListToReturn;
}

void blockbase::AddBlockDAM(eosio::name owner, eosio::name producer, blockbase::blockheaders block) {
    blockheadersIndex _blockheaders(_self, owner.value);
    currentprodIndex _currentprods(_self, owner.value);
    infoIndex _infos(_self, owner.value);
    auto info = _infos.find(owner.value);
    auto currentProducer = _currentprods.find(CKEY.value);
    
    _blockheaders.emplace(producer, [&](auto &newBlockI) {
        newBlockI.producer = block.producer;
        newBlockI.block_hash = block.block_hash;
        newBlockI.previous_block_hash = block.previous_block_hash;
        newBlockI.sequence_number = block.sequence_number;
        newBlockI.timestamp = block.timestamp;
        newBlockI.transactions_count = block.transactions_count;
        newBlockI.producer_signature = block.producer_signature;
        newBlockI.merkletree_root_hash = block.merkletree_root_hash;
        newBlockI.is_verified = false;
        newBlockI.is_latest_block = false;
        newBlockI.block_size_in_bytes = block.block_size_in_bytes;
    });

    if(std::distance(_blockheaders.begin(), _blockheaders.end()) > (info -> num_blocks_between_settlements)) _blockheaders.erase(_blockheaders.begin());
    
    _currentprods.modify(currentProducer, producer, [&](auto &currentProducerI) {
        currentProducerI.has_produced_block = true;
    });
    eosio::print("  Block valid and submitted. \n");
}

void blockbase::ResetBlockCountDAM(eosio::name owner) {
    producersIndex _producers(_self, owner.value);
    blockscountIndex _blockscount(_self, owner.value);
    for(auto producer : _producers) {
        auto producerBlockCount = _blockscount.find(producer.key.value);
        if(producerBlockCount == _blockscount.end()){
            _blockscount.emplace(owner, [&](auto &blockCountI){
                blockCountI.key = producer.key;
                blockCountI.num_blocks_failed = 0;
                blockCountI.num_blocks_produced = 0;
            });
        } else {
            _blockscount.modify(producerBlockCount, owner, [&](auto &blockCountI) {
                blockCountI.num_blocks_failed = 0;
                blockCountI.num_blocks_produced = 0;
            });
        }
    }
}

void blockbase::ReOpenCandidaturePhaseIfRequired(eosio::name owner){
    stateIndex _states(_self, owner.value);
    producersIndex _producers(_self, owner.value);
    currentprodIndex _currentprods(_self, owner.value);
    infoIndex _infos(_self, owner.value);
    auto info = _infos.find(owner.value);
    auto state = _states.find(owner.value);
    auto numberOfProducersRequired = info->number_of_validator_producers_required + info->number_of_history_producers_required + info->number_of_full_producers_required;
    uint8_t producersInSidechainCount = std::distance(_producers.begin(), _producers.end());

    if (producersInSidechainCount < numberOfProducersRequired) {
        if (producersInSidechainCount < ceil(numberOfProducersRequired * MIN_PRODUCERS_IN_CHAIN_THRESHOLD)) {
            ChangeContractStateDAM({owner, true, false, true, false, false, false, false});
            eosio::print("  Number of producers in the sidechain is below the minimum threshold, production stopped and the candidature phase is starting... \n");
            SetEndDateDAM(owner, CANDIDATURE_TIME_ID);
        } else if (producersInSidechainCount >= ceil(numberOfProducersRequired * MIN_PRODUCERS_IN_CHAIN_THRESHOLD) && producersInSidechainCount < numberOfProducersRequired) {
            if(state -> is_candidature_phase == false && state -> is_secret_sending_phase == false && state -> is_ip_sending_phase == false && state -> is_ip_retrieving_phase == false) {
                ChangeContractStateDAM({owner, true, false, true, false, false, false, true});
                SetEndDateDAM(owner, CANDIDATURE_TIME_ID);
                eosio::print("  Starting candidature time but production continues. \n");
            }
        }
    }
    eosio::print("  Producer changed. \n");
}

void blockbase::RemoveBlockCountDAM(eosio::name owner, std::vector<struct producers> producers) {
    blockscountIndex _blockscount(_self, owner.value);
    for(auto producerToRemove : producers) {
        auto producerBlockCountToRemove = _blockscount.find(producerToRemove.key.value);
        if(_blockscount.end() != producerBlockCountToRemove) {
            _blockscount.erase(producerBlockCountToRemove);
        }
    }
  
}