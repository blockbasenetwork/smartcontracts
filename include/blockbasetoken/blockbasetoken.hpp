/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/multi_index.hpp>
#include <eosio/transaction.hpp>

#include <string>

using namespace eosio;
using namespace std;


namespace eosiosystem {
    class system_contract;
}

namespace eosio {

    using std::string;

    class [[eosio::contract]] blockbasetoken : public eosio::contract {
        public:
            using contract::contract;

            const eosio::name BLOCKBASE_CONTRACT = eosio::name("blockbaseopr");

            [[eosio::action]]
            void create(const name&   issuer,
                            const asset&  maximum_supply);

            [[eosio::action]]
            void issue(const name& to, const asset& quantity, const string& memo);

            [[eosio::action]]
            void retire(const asset& quantity, const string& memo);

            [[eosio::action]]
            void transfer(const name&    from,
                              const name&    to,
                              const asset&   quantity,
                              const string&  memo);

            [[eosio::action]]
            void signup(const name& owner, const asset& quantity);

            [[eosio::action]]
            void addstake(const name& owner, const name& sidechain, const asset& stake);

            [[eosio::action]]
            void prodpunish(const name& owner,
                                 const name& contract);

            [[eosio::action]]
            void claimreward(const name& sidechain, const name& claimer);

            [[eosio::action]]
            void claimstake(const name& sidechain, const name& claimer);

            [[eosio::action]]
            void open(const name& owner, const symbol& symbol, const name& ram_payer);

            [[eosio::action]]
            void leaveledger(const name& owner, const name& sidechain);

            [[eosio::action]]
            void close(const name& owner, const symbol& symbol);

            static asset get_supply(const name& token_contract_account, const symbol_code& sym_code) {
                stats statstable(token_contract_account, sym_code.raw());
                const auto& st = statstable.get(sym_code.raw());
                return st.supply;
            }

            static asset get_balance(const name& token_contract_account, const name& owner, const symbol_code& sym_code) {
                accounts accountstable(token_contract_account, owner.value);
                const auto& ac = accountstable.get(sym_code.raw());
                return ac.balance;
            }

            static asset get_stake(const name& token_contract_account, const name& sidechain, const name& user) {
                check(is_account(user), "user account does not exist");
                check(is_account(sidechain), "sidechain does not exist");

                ledgers sidechainledger(token_contract_account, user.value);
                auto slt = sidechainledger.find(sidechain.value);
                if(slt == sidechainledger.end()){
                    asset stake = asset(-1, symbol(eosio::symbol_code("BBT"), 4));
                    return stake;
                }
                blockbasetoken::stakeledger sl = sidechainledger.get(sidechain.value);
                check(sl.owner == user, "Couldn't find user stake for given sidechain. \n");
                return sl.stake;
            }

            using create_action = eosio::action_wrapper<eosio::name("create"), &blockbasetoken::create>;
            using issue_action = eosio::action_wrapper<eosio::name("issue"), &blockbasetoken::issue>;
            using retire_action = eosio::action_wrapper<eosio::name("retire"), &blockbasetoken::retire>;
            using transfer_action = eosio::action_wrapper<eosio::name("transfer"), &blockbasetoken::transfer>;
            using signup_action = eosio::action_wrapper<eosio::name("signup"), &blockbasetoken::signup>;
            using addstake_action = eosio::action_wrapper<eosio::name("addstake"), &blockbasetoken::addstake>;
            using prodpunish_action = eosio::action_wrapper<eosio::name("prodpunish"), &blockbasetoken::prodpunish>;
            using claimreward_action = eosio::action_wrapper<eosio::name("claimreward"), &blockbasetoken::claimreward>;
            using claimstake_action = eosio::action_wrapper<eosio::name("claimstake"), &blockbasetoken::claimstake>;
            using open_action = eosio::action_wrapper<eosio::name("open"), &blockbasetoken::open>;
            using leaveledger_action = eosio::action_wrapper<eosio::name("leaveledger"), &blockbasetoken::leaveledger>;
            using close_action = eosio::action_wrapper<eosio::name("close"), &blockbasetoken::close>;

        private:
            struct [[eosio::table]] account {
                asset    balance;

                uint64_t primary_key()const { return balance.symbol.code().raw(); }
            };

            struct [[eosio::table]] currency_stats {
                asset    supply;
                asset    max_supply;
                name     issuer;

                uint64_t primary_key()const { return supply.symbol.code().raw(); }
            };

            struct [[eosio::table]] stakeledger { 
                name     sidechain;
                name     owner;
                asset    stake;    

                uint64_t primary_key()const { return sidechain.value; }
            };

            typedef eosio::multi_index<eosio::name("accounts"), account> accounts;
            typedef eosio::multi_index<eosio::name("stat"), currency_stats> stats;
            typedef eosio::multi_index<eosio::name("ledger"), stakeledger> ledgers;

            void sub_balance(const name& owner, const asset& value);
            void add_balance(const name& owner, const asset& value, const name& ram_payer);
            void sub_stake(const name& sidechain, const name& user, const asset& stake);
            void payment(const name& sidechain, const name& claimer, const asset& quantity);
    };

} /// namespace eosio
