static std::map<eosio::name, asset> GetProducersToPunishInfo(const eosio::name& contract, const eosio::name& owner) {
    blockbase::producersIndex _producers(contract, owner.value);
    blockbase::blacklistIndex _blacklists(contract, owner.value);
    std::map<eosio::name, asset> producersToPunish;
    eosio::asset stake;
    if(std::distance(_producers.begin(), _producers.end()) > 0) {
        for(auto producer : _producers){
            if(producer.warning_type == 1) {	
                stake = blockbasetoken::get_stake(BLOCKBASE_TOKEN, owner, producer.key);
                uint32_t updatedStake = (stake.amount*0.3);
                stake = asset(updatedStake, symbol(eosio::symbol_code("BBT"), 4));
                producersToPunish.insert(std::pair<eosio::name,asset>(producer.key,stake));
            }
        }
    }
    if(std::distance(_blacklists.begin(), _blacklists.end()) > 0){
        for(auto blackListedProducer : _blacklists) {
            stake = asset(1, symbol(eosio::symbol_code("BBT"), 4));
            stake = blockbasetoken::get_stake(BLOCKBASE_TOKEN, owner, blackListedProducer.key);
            if(stake.amount > 0) {
                producersToPunish.insert(std::pair<eosio::name,asset>(blackListedProducer.key,stake));
            }
        }
    }
    return producersToPunish;
}

static uint64_t GetProducerRewardAmount(eosio::name contract, eosio::name sidechain, eosio::name claimer) {
    blockbase::rewardsIndex rewards(contract, claimer.value);
    auto claimerReward = rewards.find(sidechain.value);
    return claimerReward -> reward;
}

static bool IsProducer(eosio::name contract, eosio::name owner, eosio::name producer) {
    blockbase::producersIndex _producers(contract, owner.value);
    auto producerFound = _producers.find(producer.value);
    return (producerFound != _producers.end());
}

//TODO: REVIEW
static bool IsStakeRecoverable(eosio::name contract, eosio::name owner, eosio::name producer) {
    blockbase::infoIndex _infos(contract, owner.value);
    blockbase::stateIndex _states(contract, owner.value);
    blockbase::producersIndex _producers(contract, owner.value);
    auto info   = _infos.find(owner.value);
    auto state  = _states.find(owner.value);
    auto producerInTable = _producers.find(producer.value);

    if (producerInTable == _producers.end()) return true;

    if (owner.value == producer.value){
        eosio::asset clientStake = blockbasetoken::get_stake(BLOCKBASE_TOKEN, owner, owner);
        return (clientStake.amount < ((info -> payment_per_block)*(info -> num_blocks_between_settlements))
        && state -> is_production_phase == false);
    }

    return ((producerInTable -> work_duration_in_seconds + producerInTable -> sidechain_start_date_in_seconds) <= eosio::current_block_time().to_time_point().sec_since_epoch());
}