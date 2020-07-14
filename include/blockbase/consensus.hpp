#pragma region producing Methods
void blockbase::RunSettlement(eosio::name owner) {
    infoIndex _infos(_self, owner.value);
    blockscountIndex _blockscount(_self, owner.value);
    warningsIndex _warnings(_self, owner.value);
    uint8_t producedBlocks = 0, failedBlocks = 0;
    std::vector<blockbase::producers> readyProducers = blockbase::GetReadyProducers(owner);
    int64_t totalPaymentThisSettlement = 0;
    for (struct blockbase::producers &producer : readyProducers) {
        auto producerBlockCountTable = _blockscount.find(producer.key.value);
        if (producerBlockCountTable != _blockscount.end()) {
            producedBlocks = producerBlockCountTable->num_blocks_produced;
            failedBlocks = producerBlockCountTable->num_blocks_failed;
        }
        EvaluateProducer(owner, producer.key, failedBlocks, producedBlocks);
        
        auto totalProducerPaymentPerBlockMined = CalculateRewardBasedOnBlockSize(owner,producer);
        auto producerWarningId = GetSpecificProducerWarningId(owner, producer.key, WARNING_TYPE_PUNISH);
        if(producerWarningId == -1 && producedBlocks > 0) { 
            RewardProducerDAM(owner, producer.key, totalProducerPaymentPerBlockMined);
            totalPaymentThisSettlement += totalProducerPaymentPerBlockMined;
         }
    }
    ResetBlockCountDAM(owner);
    CheckHistoryValidation(owner);
    WarningsManage(owner);
    RemoveProducerWithWorktimeFinished(owner);
    eosio::print("Computation has ended. \n");
}

void blockbase::RemoveBadProducers(eosio::name owner) {
    infoIndex _infos(_self, owner.value);
    std::vector<struct producers> punishedProducers = GetPunishedProducers(owner);
    if(punishedProducers.size() > 0){
        RemoveIPsDAM(owner, punishedProducers);
        RemoveProducersDAM(owner, punishedProducers);
        DeleteCurrentProducerDAM(owner,punishedProducers);
        RemoveBlockCountDAM(owner,punishedProducers);
        RemoveAllProducerWarningsDAM(owner, punishedProducers);
        RemoveHistVerDAM(owner, punishedProducers);
    }
}

bool blockbase::IsRequesterStakeEnough(eosio::name owner) {
    infoIndex _infos(_self, owner.value);
    auto info = _infos.find(owner.value);
    std::vector<blockbase::producers> readyProducers = blockbase::GetReadyProducers(owner);
    int64_t totalRewardsLeftToGet = 0;
    for (struct blockbase::producers &producer : readyProducers) {
        rewardsIndex _rewards(_self, producer.key.value);
        auto rewardsForProducer = _rewards.find(owner.value);
        if(rewardsForProducer != _rewards.end()) {
            totalRewardsLeftToGet += rewardsForProducer->reward;
        }
    }

    asset clientStake = blockbasetoken::get_stake(BLOCKBASE_TOKEN, owner, owner);
    int64_t clientStakeAmountUpdated = clientStake.amount - totalRewardsLeftToGet;
    auto MaxPaymentPerBlock = info->max_payment_per_block_full_producers;
    if(MaxPaymentPerBlock < info->max_payment_per_block_history_producers) MaxPaymentPerBlock = info->max_payment_per_block_history_producers;
    if(MaxPaymentPerBlock < info->max_payment_per_block_validator_producers) MaxPaymentPerBlock = info->max_payment_per_block_validator_producers;
    if (clientStakeAmountUpdated < (MaxPaymentPerBlock * (info->num_blocks_between_settlements))) {
        return false;
    }

    return true;
}


void blockbase::UpdateBlockCount(eosio::name owner, eosio::name producer) {
    blockheadersIndex _blockheaders(_self, owner.value);
    infoIndex _infos(_self, owner.value);
    blockscountIndex _blockscount(_self, owner.value);

    auto info = _infos.find(owner.value);
    auto producerBlockCount = _blockscount.find(producer.value);
    if (std::distance(_blockheaders.begin(), _blockheaders.end()) > 0) {
        if (producer == eosio::name((--_blockheaders.end())->producer) && (--_blockheaders.end())->is_verified == true) {
            _blockscount.modify(producerBlockCount, owner, [&](auto &blockCountI) {
                blockCountI.num_blocks_produced += 1;
            });
            return;
        }
    }
    _blockscount.modify(producerBlockCount, owner, [&](auto &blockcountI) {
        blockcountI.num_blocks_failed += 1;
    });


    //TODO - refactor this code to a different method
    if (std::distance(_blockheaders.begin(), _blockheaders.end()) > 0) {
        auto blockHeader = _blockheaders.find((--_blockheaders.end())->sequence_number);
        if (blockHeader->is_verified == false) {
            _blockheaders.erase(blockHeader);
            eosio::print("  Block was not validated by the production pool, so is going to be deleted. \n");
        }
    }
}

//TODO Test this method
blockbase::producers blockbase::GetNextProducer(eosio::name owner) {
    currentprodIndex _currentprods(_self, owner.value);
    producersIndex _producers(_self, owner.value);
    std::vector<blockbase::producers> readyProducers = blockbase::GetReadyProducers(owner);
    auto currentProducer = _currentprods.begin();
    struct blockbase::producers nextProducer;
    struct blockbase::producers firstProducer = _producers.get((readyProducers.front()).key.value);
    struct blockbase::producers lastProducer = _producers.get((readyProducers.back()).key.value);
 
    if (readyProducers.size() > 1 && currentProducer != _currentprods.end()) {
        auto counter = 0;
        
        for (struct blockbase::producers producer : readyProducers) {
            if (producer.key == currentProducer->producer) {
                if (producer.key == lastProducer.key) {
                    nextProducer = firstProducer;
                    return nextProducer;
                } else if (producer.key != lastProducer.key) {
                    nextProducer = readyProducers[counter + 1];
                    return nextProducer;
                }
            }
            counter++;
        }
    }
    return firstProducer;
}

void blockbase::UpdateCurrentProducerDAM(eosio::name owner, eosio::name nextProducer) {
    currentprodIndex _currentprods(_self, owner.value);
    auto currentProducer = _currentprods.find(CKEY.value);
    if (currentProducer != _currentprods.end()) {
        _currentprods.modify(currentProducer, owner, [&](auto &currentProducerI) {
            currentProducerI.producer = nextProducer;
            currentProducerI.production_start_date_in_seconds = eosio::current_block_time().to_time_point().sec_since_epoch();
            currentProducerI.has_produced_block = false;
        });
    } else {
        _currentprods.emplace(owner, [&](auto &newCurrentProducerI) {
            newCurrentProducerI.key = CKEY;
            newCurrentProducerI.producer = nextProducer;
            newCurrentProducerI.production_start_date_in_seconds = eosio::current_block_time().to_time_point().sec_since_epoch();
            newCurrentProducerI.has_produced_block = false;
        });
    }
}

void blockbase::RemoveBlockCountDAM(eosio::name owner) {
    blockscountIndex _blockscount(_self, owner.value);
    if (std::distance(_blockscount.begin(), _blockscount.end()) != 0) {
        auto itr = _blockscount.begin();
        while (itr != _blockscount.end()) itr = _blockscount.erase(itr);	
    }
}

uint8_t blockbase::CalculateMultiSigThreshold(uint8_t numberOfProducers){
    return ceil(numberOfProducers / 2) + 1;
}

void blockbase::DeleteCurrentProducerDAM(eosio::name owner, std::vector<struct producers> producersToRemove) {
    currentprodIndex _currentprods(_self, owner.value);
    auto currentProducer = _currentprods.find(CKEY.value);
    for(auto producers: producersToRemove) {
        if(producers.key == currentProducer -> producer) {
            _currentprods.erase(currentProducer);
        }
    }
}

void blockbase::RemoveProducerWithWorktimeFinished(eosio::name owner){
    producersIndex _producers(_self, owner.value);
    warningsIndex _warnings(_self, owner.value);
    std::vector<struct producers> producersToRemove;
    for(auto producer : _producers) {
        if(producer.work_duration_in_seconds <= eosio::current_block_time().to_time_point().sec_since_epoch()) {        
            auto warningsByExitTime = _warnings.get_index<"byexittime"_n>(); // If is producer makes sense to order by exittime due to the producer_exit_date_in_seconds being 0
            for(auto warning : warningsByExitTime) {
                if(warning.producer == producer.key) {
                    _warnings.modify(warning, owner, [&](auto &warningI) {
                        warningI.producer_exit_date_in_seconds = eosio::current_block_time().to_time_point().sec_since_epoch();
                    });
                    eosio::print("Exit date registed for the ", producer.key);
                }
            }
            producersToRemove.push_back(producer);
            eosio::print("Producer ", producer.key," is leaving the sidechain with no penalties but any warning will be kept. His work time as expired.");
        }
    }
    if(std::distance(producersToRemove.begin(), producersToRemove.end()) > 0) {
        RemoveIPsDAM(owner, producersToRemove);
        RemoveProducersDAM(owner, producersToRemove);
        DeleteCurrentProducerDAM(owner,producersToRemove);
        RemoveBlockCountDAM(owner,producersToRemove);
    }
}

void blockbase::WarningsManage(eosio::name owner) {
    warningsIndex _warnings(_self, owner.value);
    std::map<uint64_t, eosio::name> warningIdProducerNameMapToClear;
    for(auto warning : _warnings) {
        // Case the warning of the producer stays 5 days with the same warning the producer is flagged to be banned
        if(warning.producer_exit_date_in_seconds == 0 && eosio::current_block_time().to_time_point().sec_since_epoch() - warning.warning_creation_date_in_seconds >= 500) { // 432000 is 5 days in seconds
            AddWarningDAM(owner, warning.producer, WARNING_TYPE_PUNISH);
        
        // Case 20 days has pass since the producer as exit the sidechain the warning is cleared.
        } else if(warning.producer_exit_date_in_seconds != 0 && eosio::current_block_time().to_time_point().sec_since_epoch() - warning.producer_exit_date_in_seconds >= 600) { // 1728000 is 20 days in seconds
            warningIdProducerNameMapToClear.insert(std::pair<uint64_t, eosio::name>(warning.key, warning.producer));
        }
    }

    for(auto const& warningIdProducerNamePairToClear : warningIdProducerNameMapToClear) {
        ClearWarningDAM(owner, warningIdProducerNamePairToClear.second, warningIdProducerNamePairToClear.first);
    }
}

std::vector<blockbase::warnings> blockbase::GetAllProducerWarnings(eosio::name owner, eosio::name producer) {
    warningsIndex _warnings(_self, owner.value);
    std::vector<blockbase::warnings> warninglist;
    for(auto warning : _warnings) {
        if(warning.producer == producer) 
            warninglist.push_back(warning); 
    }
    return warninglist;
}

int64_t blockbase::GetSpecificProducerWarningId(eosio::name owner,eosio::name producer, uint8_t warningType) {
    warningsIndex _warnings(_self, owner.value);
    for(auto warning : _warnings) {
        if(warning.producer == producer && warning.warning_type == warningType) 
            return warning.key;
    }
    return -1;
}
#pragma endregion
