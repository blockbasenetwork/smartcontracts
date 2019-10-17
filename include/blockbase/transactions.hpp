    void blockbase::actiondeploy(eosio::name actionname, eosio::name scope, eosio::name permission) {
        action(
            permission_level{ scope, permission },
            _self, actionname,
            std::make_tuple(scope)
        ).send();
    }