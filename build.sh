eosio-cpp -I ./include src/blockbase.cpp -o ~/eos/contract/blockbase/blockbase.wasm -abigen --contract=blockbase
eosio-cpp -I ./include src/blockbasetoken.cpp -o ~/eos/contract/blockbasetoken/blockbasetoken.wasm -abigen --contract=blockbasetoken
