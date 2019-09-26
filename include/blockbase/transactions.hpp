    void blockbase::actiondeploy(eosio::name actionname, eosio::name scope, eosio::name permission) {
        action(
            permission_level{ scope, permission },
            _self, actionname,
            std::make_tuple(scope)
        ).send();
    }

    void blockbase::delayedtx(eosio::name owner, eosio::name action, uint16_t delay, uint32_t uniqueidaddition) {
        eosio::transaction txn{};
        txn.actions.emplace_back(
            permission_level{owner, eosio::name("active") },
            _self, action,
            std::make_tuple(owner)
        );
        txn.delay_sec = delay;
        txn.send(owner.value + uniqueidaddition, owner);
    }