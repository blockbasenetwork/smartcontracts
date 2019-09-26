eosio-cpp -I ./include src/blockbase.cpp -o ~/eos/contracts/blockbase/blockbase.wasm -abigen --contract=blockbase
eosio-cpp -I ./include src/blockbasetoken.cpp -o ~/eos/contracts/blockbasetoken/blockbasetoken.wasm -abigen --contract=blockbasetoken
