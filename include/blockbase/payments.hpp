#pragma region Payment Methods

void blockbase::rewardprod(eosio::name owner ,eosio::name producer, uint16_t quantity) {
    rewardsIndex _rewards(_self, producer.value);
    auto rewardsforproducer = _rewards.find(producer.value);
    if(rewardsforproducer == _rewards.end()) {
        _rewards.emplace(owner, [&](auto &reward) {
            reward.key = producer;
            reward.reward = quantity;
        });
    } else {
        _rewards.modify(rewardsforproducer, owner, [&](auto &reward) {
            reward.reward += quantity;
        });
    }
    eosio::print("  Reward registed. \n");
}

#pragma endregion
