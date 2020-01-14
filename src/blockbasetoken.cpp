#include <cmath>
#include <blockbasetoken/blockbasetoken.hpp>
#include <blockbase/blockbase.hpp>
#include <native.hpp>

#include <blockbase/service.hpp>

void blockbasetoken::create(const name& issuer, const asset& maximum_supply) {
    require_auth(get_self());

    auto sym = maximum_supply.symbol;
    check(sym.is_valid(), "invalid symbol name");
    check(maximum_supply.is_valid(), "invalid supply");
    check(maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable(get_self(), sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    check(existing == statstable.end(), "token with symbol already exists");

    statstable.emplace(get_self(), [&](auto& s) {
        s.supply.symbol = maximum_supply.symbol;
        s.max_supply    = maximum_supply;
        s.issuer        = issuer;
    });
}

void blockbasetoken::issue(const name& to, const asset& quantity, const string& memo) {
    auto sym = quantity.symbol;
    check(sym.is_valid(), "invalid symbol name");
    check(memo.size() <= 256, "memo has more than 256 bytes");

    stats statstable(get_self(), sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    check(existing != statstable.end(), "token with symbol does not exist, create token before issue");
    const auto& st = *existing;
    check( to == st.issuer, "tokens can only be issued to issuer account" );

    require_auth(st.issuer);
    check(quantity.is_valid(), "invalid quantity");
    check(quantity.amount > 0, "must issue positive quantity");

    check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
    check(quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify(st, same_payer, [&](auto& s) {
        s.supply += quantity;
    });

    add_balance(st.issuer, quantity, st.issuer);
}

void blockbasetoken::retire(const asset& quantity, const string& memo) {
    auto sym = quantity.symbol;
    check(sym.is_valid(), "invalid symbol name");
    check(memo.size() <= 256, "memo has more than 256 bytes");

    stats statstable(get_self(), sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    check(existing != statstable.end(), "token with symbol does not exist");
    const auto& st = *existing;

    require_auth(st.issuer);
    check(quantity.is_valid(), "invalid quantity");
    check(quantity.amount > 0, "must retire positive quantity");

    check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");

    statstable.modify(st, same_payer, [&](auto& s) {
        s.supply -= quantity;
    });

    sub_balance(st.issuer, quantity);
}

void blockbasetoken::transfer(const name& from, const name& to, const asset& quantity, const string& memo) {
    check(from != to, "cannot transfer to self");
    require_auth(from);
    check(is_account(to), "to account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable(get_self(), sym.raw());
    const auto& st = statstable.get(sym.raw());

    require_recipient(from);
    require_recipient(to);

    check(quantity.is_valid(), "invalid quantity");
    check(quantity.amount > 0, "must transfer positive quantity");
    check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
    check(memo.size() <= 256, "memo has more than 256 bytes");

    auto payer = has_auth(to) ? to : from;

    sub_balance(from, quantity);
    add_balance(to, quantity, payer);
}

void blockbasetoken::signup(const name& owner, const asset& quantity) {
    check(eosio::current_block_time().to_time_point().sec_since_epoch() > 1570550400, "Time for Airgrab didn't start yet!");
    auto sym = quantity.symbol;
    check(sym.is_valid(), "Invalid symbol name");

    auto sym_code_raw = sym.code().raw();

    stats statstable(_self, sym_code_raw);
    auto existing = statstable.find(sym_code_raw);
    check(existing != statstable.end(), "Token with that symbol name does not exist - Please create the token before issuing");

    const auto& st = *existing;

    require_auth(owner);
    require_recipient(owner);

    accounts to_acnts(_self, owner.value);
    auto to = to_acnts.find(sym_code_raw);
    check(to == to_acnts.end(), "You have already signed up");

    check(quantity.is_valid(), "Invalid quantity value");
    check(quantity.amount == 0, "Quantity exceeds signup allowance");
    check(st.supply.symbol == quantity.symbol, "Symbol precision mismatch");
    check(st.max_supply.amount - st.supply.amount >= quantity.amount, "Quantity value cannot exceed the available supply");

    statstable.modify(st, same_payer, [&](auto& s) {
        s.supply += quantity;
    });

    add_balance(owner, quantity, owner);
}

void blockbasetoken::sub_balance(const name& owner, const asset& value) {
    accounts from_acnts(get_self(), owner.value);

    const auto& from = from_acnts.get(value.symbol.code().raw(), "no balance object found");
    check(from.balance.amount >= value.amount, "overdrawn balance");

    from_acnts.modify(from, owner, [&](auto& a) {
            a.balance -= value;
        });
}

void blockbasetoken::add_balance(const name& owner, const asset& value, const name& ram_payer) {
    accounts to_acnts(get_self(), owner.value);
    auto to = to_acnts.find(value.symbol.code().raw());
    if(to == to_acnts.end()) {
        to_acnts.emplace(ram_payer, [&](auto& a) {
          a.balance = value;
        });
    } else {
        to_acnts.modify(to, same_payer, [&](auto& a) {
          a.balance += value;
        });
    }
}

void blockbasetoken::payment(const name& sidechain, const name& claimer, const asset& quantity) {
    check(sidechain != claimer, "cannot transfer to self");
    check(is_account(claimer), "claimer account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable(get_self(), sym.raw());
    const auto& st = statstable.get(sym.raw());

    require_recipient(sidechain);
    require_recipient(claimer);

    check(quantity.is_valid(), "invalid quantity");
    check(quantity.amount > 0, "must transfer positive quantity");
    check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");

    auto payer = has_auth(claimer) ? claimer : sidechain;

    sub_stake(sidechain, sidechain, quantity);
    add_balance(claimer, quantity, payer);
}

void blockbasetoken::addstake(const name& owner, const name& sidechain, const asset& stake) {
    require_auth(owner);
    check(is_account(sidechain), "sidechain account does not exist");
    auto sym = stake.symbol.code();
    stats statstable(get_self(), sym.raw());
    const auto& st = statstable.get(sym.raw());

    require_recipient(owner);
    require_recipient(sidechain);

    check(stake.is_valid(), "invalid stake quantity");
    check(stake.amount > 0, "must transfer positive stake");
    check(stake.symbol == st.supply.symbol, "symbol precision mismatch");

    ledgers sidechainledger(get_self(), owner.value);
    auto sd = sidechainledger.find(sidechain.value);
    if(sd == sidechainledger.end()){
        sidechainledger.emplace(owner, [&](auto& s) {
            s.sidechain = sidechain;
            s.owner = owner;
            s.stake = stake;
        });
    } else {
        sidechainledger.modify(sd, same_payer, [&](auto& s) {
            s.stake += stake;
        });
    }
    sub_balance(owner, stake);
}

void blockbasetoken::sub_stake(const name& sidechain, const name& user, const asset& stake) {
    ledgers sidechainledger(get_self(), user.value);

    auto sd = sidechainledger.find(sidechain.value);
    check(sd->stake.amount >= stake.amount, "overdrawn stake");
    check(sd->owner == user, "invalid user");

    sidechainledger.modify(sd, same_payer, [&](auto& s) {
            s.stake -= stake;
    });
}

void blockbasetoken::prodpunish(const name& owner, const name& contract){
    require_auth(owner);
    std::map<eosio::name, asset> prodpunish = GetProducersToPunishInfo(contract, owner);
    if(prodpunish.size() > 0){
        for (auto const& producer : prodpunish) {
        
            check(is_account(producer.first), "Producer does not exist");
            auto sym = producer.second.symbol.code();
            stats statstable(get_self(), sym.raw());
            const auto& st = statstable.get(sym.raw());

            require_recipient(producer.first);
            require_recipient(owner);

            check(producer.second.is_valid(), "invalid quantity");
            check(producer.second.amount > 0, "must transfer positive quantity");
            check(producer.second.symbol == st.supply.symbol, "symbol precision mismatch");

            sub_stake(owner, producer.first, producer.second);

            ledgers sidechainledger(get_self(), owner.value);
            auto sd = sidechainledger.find(owner.value);

            sidechainledger.modify(sd, owner, [&](auto& s) {
                    s.stake += producer.second;
            });
        }
    }
}

void blockbasetoken::claimreward(const name& sidechain, const name& claimer) {
    require_auth(claimer);
    require_recipient(claimer);

    ledgers sidechainledger(get_self(), claimer.value);
    for(auto& ledger : sidechainledger){
        if(ledger.sidechain == sidechain){
            auto reward = GetProducerRewardAmount(BLOCKBASE_CONTRACT, claimer);
            asset payment_reward = asset(reward, symbol(symbol_code("BBT"), 4));
            action(
                permission_level{get_self(), eosio::name("active")},
                BLOCKBASE_CONTRACT, eosio::name("resetreward"),
                std::make_tuple(ledger.sidechain, claimer)
            ).send();
            payment(ledger.sidechain, claimer, payment_reward);
            if(!(IsProducer(BLOCKBASE_CONTRACT, ledger.sidechain, claimer))){
                sidechainledger.erase(ledger);
            }
        }
    }
}

void blockbasetoken::claimstake(const name& sidechain, const name& claimer) {
    require_auth(claimer);
    check(is_account(sidechain), "sidechain account does not exist");

    require_recipient(claimer);
    require_recipient(sidechain);

    ledgers sidechainledger(get_self(), claimer.value);
    auto ledger = sidechainledger.find(sidechain.value);
    check(ledger -> stake.is_valid(), "invalid stake quantity");
    check(ledger -> stake.amount > 0, "must transfer positive stake");

    if(ledger -> owner == claimer && IsStakeRecoverable(BLOCKBASE_CONTRACT, sidechain, claimer)){
        sub_stake(sidechain, claimer, ledger -> stake);
        add_balance(claimer, ledger -> stake, claimer);
    }
}

void blockbasetoken::open(const name& owner, const symbol& symbol, const name& ram_payer) {
    require_auth(ram_payer);

    check(is_account(owner), "owner account does not exist");

    auto sym_code_raw = symbol.code().raw();
    stats statstable(get_self(), sym_code_raw);
    const auto& st = statstable.get(sym_code_raw, "symbol does not exist");
    check(st.supply.symbol == symbol, "symbol precision mismatch");

    accounts acnts(get_self(), owner.value);
    auto it = acnts.find(sym_code_raw);
    if(it == acnts.end()) {
        acnts.emplace(ram_payer, [&](auto& a){
          a.balance = asset{0, symbol};
        });
    }
}

void blockbasetoken::leaveledger(const name& owner, const name& producer, const name& sidechain){
    require_auth(owner);
    check(is_account(producer), "sidechain account does not exist");

    require_recipient(owner);
    require_recipient(producer);

    ledgers sidechainledger(get_self(), producer.value);
    auto sd = sidechainledger.find(sidechain.value);

    if(sd != sidechainledger.end() && sd -> stake.amount <= 0){
        sidechainledger.erase(sd);
    } 
}

void blockbasetoken::close(const name& owner, const symbol& symbol) {
    require_auth(owner);
    accounts acnts(get_self(), owner.value);
    auto it = acnts.find(symbol.code().raw());
    check(it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect.");
    check(it->balance.amount == 0, "Cannot close because the balance is not zero.");
    acnts.erase(it);
}