#include <native.hpp>

    void blockbase::authassign(eosio::name owner, eosio::name newpermission, eosio::name permission1 , uint8_t threshold){
        eosiosystem::authority newauth;
        newauth.threshold = threshold;
        producersIndex _producers(_self, owner.value);
        if(std::distance(_producers.begin(), _producers.end()) > 0){
            for(producers accountname : _producers){
                eosio::permission_level permission(accountname.key, permission1);
                eosiosystem::permission_level_weight accountpermission{permission, 1};
                newauth.accounts.emplace_back(accountpermission);
            }
        } else {
            eosio::permission_level permission(owner, permission1);
            eosiosystem::permission_level_weight accountpermission{permission, 1};
            newauth.accounts.emplace_back(accountpermission);
        }
        // Send off the action to updateauth
        eosio::action(
            eosio::permission_level(owner,
            eosio::name("active")),
            eosio::name("eosio"),
            eosio::name("updateauth"),
            std::tuple(owner,
            newpermission,
            eosio::name("active"),
            newauth)).send();
    }

    void blockbase::linkauth(eosio::name contract, std::vector<eosio::name> actions, eosio::name permission) {
        for(eosio::name action : actions) {
            eosio::action(
                eosio::permission_level(contract,
                eosio::name("active")),
                eosio::name("eosio"),
                eosio::name("linkauth"),
                std::tuple(contract,
                get_self(),
                action,
                permission)).send();
        }
    }