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
    RemoveUnreadyProducersWithLongWaitTime(owner);
    UpdateConfigurations(owner);
    RemoveExtraProducers(owner);
    eosio::print("Computation has ended. \n");
}

void blockbase::UpdateConfigurations(eosio::name owner) {
    configchgIndex _configchange(_self, owner.value);
    auto changeInfo = _configchange.find(owner.value);
    if(changeInfo == _configchange.end()) return;
    if(changeInfo->config_changed_time_in_seconds > (eosio::current_block_time().to_time_point().sec_since_epoch() - ONE_DAY_IN_SECONDS)) return;

    infoIndex _infos(_self, owner.value);
    auto info = _infos.find(owner.value);

    if (info != _infos.end()) {
        _infos.modify(info, owner, [&](auto &infoI) {
            infoI.max_payment_per_block_validator_producers = changeInfo->max_payment_per_block_validator_producers;
            infoI.max_payment_per_block_history_producers = changeInfo->max_payment_per_block_history_producers;
            infoI.max_payment_per_block_full_producers = changeInfo->max_payment_per_block_full_producers;
            infoI.min_payment_per_block_validator_producers = changeInfo->min_payment_per_block_validator_producers;
            infoI.min_payment_per_block_history_producers = changeInfo->min_payment_per_block_history_producers;
            infoI.min_payment_per_block_full_producers = changeInfo->min_payment_per_block_full_producers;
            infoI.min_candidature_stake = changeInfo->min_candidature_stake;
            infoI.number_of_validator_producers_required = changeInfo->number_of_validator_producers_required;
            infoI.number_of_history_producers_required = changeInfo->number_of_history_producers_required;
            infoI.number_of_full_producers_required = changeInfo->number_of_full_producers_required;
            infoI.block_time_in_seconds = changeInfo->block_time_in_seconds;
            infoI.num_blocks_between_settlements = changeInfo->num_blocks_between_settlements;
            infoI.block_size_in_bytes = changeInfo->block_size_in_bytes;
        });
    }

    _configchange.erase(changeInfo);
}

void blockbase::RemoveExtraProducers(eosio::name owner) {
    infoIndex _infos(_self, owner.value);
    producersIndex _producers(_self, owner.value);
    reservedseatIndex _reservedseats(_self, owner.value);
    auto info = _infos.find(owner.value);

    auto producersInSidechainCount = std::distance(_producers.begin(), _producers.end());
    auto reservedSeatsCount = std::distance(_reservedseats.begin(), _reservedseats.end());
    auto numberOfProducersRequired = info->number_of_validator_producers_required + info->number_of_history_producers_required + info->number_of_full_producers_required + reservedSeatsCount;

    if (numberOfProducersRequired >= producersInSidechainCount) return;

    std::vector<struct producers> producersToRemove = GetAllProducersToRemove(owner, producersInSidechainCount - numberOfProducersRequired);
    if(producersToRemove.size() > 0){
        RemoveIPsDAM(owner, producersToRemove);
        RemoveProducersDAM(owner, producersToRemove);
        DeleteCurrentProducerDAM(owner,producersToRemove);
        RemoveBlockCountDAM(owner,producersToRemove);
        RemoveAllProducerWarningsDAM(owner, producersToRemove);
        RemoveHistVerDAM(owner, producersToRemove);
    }
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


void blockbase::UpdateBlockCount(eosio::name owner, eosio::name producer, uint64_t roundStartTime) {
    blockheadersIndex _blockheaders(_self, owner.value);
    infoIndex _infos(_self, owner.value);
    blockscountIndex _blockscount(_self, owner.value);
    blockcheckIndex _blockcheck(_self, owner.value);

    auto blockcheck = _blockcheck.find(BLCKEY.value);
    auto info = _infos.find(owner.value);
    auto producerBlockCount = _blockscount.find(producer.value);
    auto lastBlockTimeChecksOut = (--_blockheaders.end())->timestamp > roundStartTime;
    if (std::distance(_blockheaders.begin(), _blockheaders.end()) > 0) {
        if (producer == eosio::name((--_blockheaders.end())->producer) && lastBlockTimeChecksOut && (--_blockheaders.end())->is_verified == true) {
            _blockscount.modify(producerBlockCount, owner, [&](auto &blockCountI) {
                blockCountI.num_blocks_produced += 1;
            });
            return;
        }
    }
    if (!lastBlockTimeChecksOut || blockcheck->block_hash != (--_blockheaders.end())->block_hash || blockcheck->timestamp > (roundStartTime + info->block_time_in_seconds) / 3) {
        _blockscount.modify(producerBlockCount, owner, [&](auto &blockcountI) {
            blockcountI.num_blocks_failed += 1;
        });
    }
    


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

void blockbase::UpdateBlockCheckDAM(eosio::name owner, eosio::name producer, std::string blockHash) {
    blockcheckIndex _blockcheck(_self, owner.value);
    auto blockcheck = _blockcheck.find(BLCKEY.value);
    if (blockcheck != _blockcheck.end()) {
        _blockcheck.modify(blockcheck, producer, [&](auto &blockcheckI) {
            blockcheckI.timestamp = eosio::current_block_time().to_time_point().sec_since_epoch();
            blockcheckI.block_hash = blockHash;
        });
    } else {
        _blockcheck.emplace(producer, [&](auto &blockcheckI) {
            blockcheckI.key = BLCKEY;
            blockcheckI.timestamp = eosio::current_block_time().to_time_point().sec_since_epoch();
            blockcheckI.block_hash = blockHash;
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
            for(auto warning : _warnings) {
                if(warning.producer == producer.key) {
                    auto warningToModify = _warnings.find(warning.key);
                    _warnings.modify(warningToModify, owner, [&](auto &warningI) {
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
        RemoveHistVerDAM(owner, producersToRemove);
    }
}

void blockbase::RemoveUnreadyProducersWithLongWaitTime(eosio::name owner){
    producersIndex _producers(_self, owner.value);
    std::vector<struct producers> producersToRemove;
    for(auto producer : _producers) {
        if(!producer.is_ready_to_produce && producer.sidechain_start_date_in_seconds + (ONE_DAY_IN_SECONDS * 3) <= eosio::current_block_time().to_time_point().sec_since_epoch()) {        
            producersToRemove.push_back(producer);
        }
    }
    if(std::distance(producersToRemove.begin(), producersToRemove.end()) > 0) {
        RemoveIPsDAM(owner, producersToRemove);
        RemoveProducersDAM(owner, producersToRemove);
        RemoveBlockCountDAM(owner,producersToRemove);
        RemoveHistVerDAM(owner, producersToRemove);
    }
}

void blockbase::WarningsManage(eosio::name owner) {
    warningsIndex _warnings(_self, owner.value);
    std::map<uint64_t, eosio::name> warningIdProducerNameMapToClear;
    for(auto warning : _warnings) {
        // Case the warning of the producer stays 5 days with the same warning the producer is flagged to be banned
        if(warning.producer_exit_date_in_seconds == 0 && eosio::current_block_time().to_time_point().sec_since_epoch() - warning.warning_creation_date_in_seconds >= (ONE_DAY_IN_SECONDS * 5)) {
            AddWarningDAM(owner, warning.producer, WARNING_TYPE_PUNISH);
        
        // Case 20 days has pass since the producer as exit the sidechain the warning is cleared.
        } else if(warning.producer_exit_date_in_seconds != 0 && eosio::current_block_time().to_time_point().sec_since_epoch() - warning.producer_exit_date_in_seconds >= (ONE_DAY_IN_SECONDS * 20)) {
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

std::vector<struct blockbase::producers> blockbase::GetAllProducersToRemove(eosio::name owner, uint16_t numberOfProducersToRemove) {
    producersIndex _producers(_self, owner.value);
    std::vector<struct blockbase::producers> producers;
    std::vector<struct blockbase::producers> producersToRemove;
    for(auto& producer : _producers) {
        producers.push_back(producer);
    }

    return producers;
}

#pragma endregion
