#pragma region Payment Methods

void blockbase::RewardProducerDAM(eosio::name owner ,eosio::name producer, uint64_t quantity) {
    rewardsIndex _rewards(_self, producer.value);
    auto rewardsForProducer = _rewards.find(producer.value);
    if(rewardsForProducer == _rewards.end()) {
        _rewards.emplace(owner, [&](auto &reward) {
            reward.key = producer;
            reward.reward = quantity;
        });
    } else {
        _rewards.modify(rewardsForProducer, owner, [&](auto &reward) {
            reward.reward += quantity;
        });
    }
    eosio::print("  Reward registed. \n");
}

#pragma endregion
