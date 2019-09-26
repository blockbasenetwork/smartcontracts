const eosio::name BLOCKBASE_TOKEN = eosio::name("blockbasetkn");

static std::map<eosio::name, asset> getbadprods(const eosio::name& contract, const eosio::name& owner) {
    blockbase::producersIndex _producers(contract, owner.value);
    blockbase::blacklistIndex _blacklists(contract, owner.value);
    std::map<eosio::name, asset> producerstopunish;
    eosio::asset stake;
    if(std::distance(_producers.begin(), _producers.end()) > 0) {
        for(auto producer : _producers){
            if(producer.warning == 1) {	
                stake = blockbasetoken::get_stake(BLOCKBASE_TOKEN, owner, producer.key);
                uint32_t updatedstake = (stake.amount*0.3);
                stake = asset(updatedstake, symbol(eosio::symbol_code("BBT"), 4));
                producerstopunish.insert(std::pair<eosio::name,asset>(producer.key,stake));
            }
        }
    }
    if(std::distance(_blacklists.begin(), _blacklists.end()) > 0){
        for(auto blacklisted : _blacklists) {
            stake = asset(1, symbol(eosio::symbol_code("BBT"), 4));
            stake = blockbasetoken::get_stake(BLOCKBASE_TOKEN, owner, blacklisted.key);
            if(stake.amount > 0) {
                producerstopunish.insert(std::pair<eosio::name,asset>(blacklisted.key,stake));
            }
        }
    }
    return producerstopunish;
}

static uint64_t getreward(eosio::name contract, eosio::name claimer) {
    blockbase::rewardsIndex rewards(contract, claimer.value);
    auto claimreward = rewards.find(claimer.value);
    return claimreward -> reward;
}

static bool isprod(eosio::name contract, eosio::name owner, eosio::name producer) {
    blockbase::producersIndex _producers(contract, owner.value);
    auto producerfound = _producers.find(producer.value);
    return (producerfound != _producers.end());
}

//TODO: REVIEW
static bool isstakerecoverable(eosio::name contract, eosio::name owner, eosio::name producer) {
    blockbase::infoIndex _infos(contract, owner.value);
    blockbase::stateIndex _states(contract, owner.value);
    blockbase::producersIndex _producers(contract, owner.value);
    auto info   = _infos.find(owner.value);
    auto state  = _states.find(owner.value);
    auto producerI = _producers.find(producer.value);

    if (producerI == _producers.end()) return true;

    if (owner.value == producer.value){
        eosio::asset clientstake = blockbasetoken::get_stake(eosio::name("bbtoken"), owner, owner);
        return (clientstake.amount < ((info -> paymentperblock)*(info -> blocksbetweensettlement))
        && state -> productiontime == false);
    }

    return ((producerI -> worktimeinseconds + producerI -> startinsidechaindate) <= eosio::current_block_time().to_time_point().sec_since_epoch());
}