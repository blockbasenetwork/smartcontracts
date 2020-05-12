#pragma region Payment Methods

void blockbase::RewardProducerDAM(eosio::name owner ,eosio::name producer, uint64_t quantity) {
    rewardsIndex _rewards(_self, producer.value);
    auto rewardsForProducer = _rewards.find(owner.value);
    if(rewardsForProducer == _rewards.end()) {
        _rewards.emplace(owner, [&](auto &reward) {
            reward.key = owner;
            reward.reward = quantity;
        });
    } else {
        _rewards.modify(rewardsForProducer, owner, [&](auto &reward) {
            reward.reward += quantity;
        });
    }
    eosio::print("  Reward registed. \n");
}

uint64_t blockbase::CalculateRewardBasedOnBlockSize(eosio::name owner, struct blockbase::producers producer) {
    infoIndex _infos(_self, owner.value);
    blockheadersIndex _blockheaders(_self, owner.value);
    auto contractInfo = _infos.find(owner.value);
    auto maxBlockSizeByte = contractInfo->block_size_in_bytes;
    auto paymentToProducer = 0;

    for (auto &blockheader : _blockheaders) {
        if(eosio::name(blockheader.producer) == producer.key) {
            auto producerTypeMaxPayment = producer.producer_type == 1 ? contractInfo->max_payment_per_block_validator_producers : producer.producer_type == 2 ? contractInfo->max_payment_per_block_history_producers : contractInfo->max_payment_per_block_full_producers;
            auto producerTypeMinPayment = producer.producer_type == 1 ? contractInfo->min_payment_per_block_validator_producers : producer.producer_type == 2 ? contractInfo->min_payment_per_block_history_producers : contractInfo->min_payment_per_block_full_producers;
            auto paymentDiff =  producerTypeMaxPayment - producerTypeMinPayment;
            auto calculatedPaymentBeforeAddingMinValue = (paymentDiff * blockheader.block_size_in_bytes)/maxBlockSizeByte;
            paymentToProducer += producerTypeMinPayment + calculatedPaymentBeforeAddingMinValue;
        }
    }
    return paymentToProducer;
}
#pragma endregion
