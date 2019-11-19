using namespace eosio;

static const eosio::name BLOCKBASE_TOKEN = eosio::name("jungletttt33");
static const std::string BLOCKBASE_TOKEN_SYMBOL = "BBT";

class[[eosio::contract]] blockbase : public eosio::contract {

  public:
    blockbase(eosio::name receiver, eosio::name code, eosio::datastream<const char *> ds) : eosio::contract(receiver, code, ds) {}

#pragma region Global Variabels
    // Minimum
    const uint32_t MIN_PAYMENT = 0;
    const uint32_t MIN_WORKDAYS_IN_SECONDS = 1;
    const uint32_t MIN_CANDIDATE_STAKE = 1;
    const uint32_t MIN_STAKE_FOR_CLIENT = 1;
    const uint16_t MIN_CANDIDATURE_TIME = 1;
    const uint16_t MIN_IP_SEND_TIME = 1;
    const uint8_t MIN_PRODUCERS = 1;
    // Warning
    const uint8_t WARNING_CLEAR = 0;
    const uint8_t WARNING_FLAGGED = 1;
    const uint8_t WARNING_PUNISH = 2;

    // Contract Info
    const uint8_t NUMBER_ENCRYPTED_IPS = 3;
    const double PRODUCERS_IN_CHAIN_THRESHOLD = 0.40;
    const double FLAGGED_PERCENTAGE = 0.3;
    const double THRESHOLD_FOR_PUNISH = 0.70;

    const uint8_t CHANGE_PRODUCER_ID = 0;
    const uint8_t CANDIDATURE_TIME_ID = 1;
    const uint8_t SECRET_TIME_ID = 2;
    const uint8_t SEND_TIME_ID = 3;
    const uint8_t RECEIVE_TIME_ID = 4;
    const uint8_t PRODUCTION_TIME_ID = 5;

    const eosio::name VERIFY_PERMISSION_NAME = eosio::name("verifyblock");
    const std::vector<eosio::name> VERIFY_PERMISSION_ACTION{eosio::name("verifyblock")};
    const eosio::name CKEY = eosio::name("currentprod");

#pragma endregion
#pragma region Contract Tables

    // Producers Table
    struct [[eosio::table]] producers {
        eosio::name key;
        std::string publickey;
        uint8_t warning;
        uint64_t worktimeinseconds;
        uint64_t startinsidechaindate;
        bool isready;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("producers"), producers> producersIndex;

    // Candidates Table
    struct [[eosio::table]] candidates {
        eosio::name key;
        std::string publickey;
        checksum256 secrethash;
        checksum256 secret;
        uint64_t worktimeinseconds;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("candidates"), candidates> candidatesIndex;

    // Blockheaders Table
    struct [[eosio::table]] blockheaders {
        std::string producer;
        std::string blockhash;
        std::string previousblockhash;
        uint64_t sequencenumber;
        uint64_t timestamp;
        std::string producersignature;
        std::string merkletreeroothash;
        bool isverified;
        bool islastblock;
        uint64_t primary_key() const { return sequencenumber; }
    };
    typedef eosio::multi_index<eosio::name("blockheaders"), blockheaders> blockheadersIndex;

    // Client Table
    struct [[eosio::table]] client {
        eosio::name key;
        std::string publickey;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("client"), client> clientIndex;

    // Ip addresses Table
    struct [[eosio::table]] ipaddress {
        eosio::name key;
        std::string publickey;
        std::vector<std::string> encryptedips;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("ipaddress"), ipaddress> ipsIndex;

    // Contract State Table
    struct [[eosio::table]] contractst {
        eosio::name key;
        bool startchain;
        bool configtime;
        bool candidaturetime;
        bool secrettime;
        bool ipsendtime;
        bool ipreceivetime;
        bool productiontime;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("contractst"), contractst> stateIndex;

    // Contract Information Table
    struct [[eosio::table]] contractinfo {
        eosio::name key;
        uint32_t paymentperblock;
        uint32_t minimumcandidatestake;
        uint32_t requirednumberofproducers;
        uint32_t candidaturetime;
        uint32_t sendsecrettime;
        uint32_t ipsendtime;
        uint32_t ipreceivetime;
        uint32_t candidatureenddate;
        uint32_t secretenddate;
        uint32_t ipsendenddate;
        uint32_t ipreceiveenddate;
        uint16_t blocktimeduration;
        uint8_t blocksbetweensettlement;
        uint64_t sizeofblockinbytes;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("contractinfo"), contractinfo> infoIndex;

    // BlackList Table
    struct [[eosio::table]] blacklist {
        eosio::name key;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("blacklist"), blacklist> blacklistIndex;

    // Block Count Table
    struct [[eosio::table]] blockscount {
        eosio::name key;
        uint8_t blocksfailed;
        uint8_t blocksproduced;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("blockscount"), blockscount> blockscountIndex;

    // Current Producer Table
    struct [[eosio::table]] currentprod {
        eosio::name id;
        eosio::name producer;
        uint64_t startproductiontime;
        bool isblockproduced;
        uint64_t primary_key() const { return id.value; }
    };
    typedef eosio::multi_index<eosio::name("currentprod"), currentprod> currentprodIndex;

    // Rewards Producer Table
    struct [[eosio::table]] pendingrewards {
        eosio::name key;
        uint64_t reward;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("rewards"), pendingrewards> rewardsIndex;

#pragma endregion
#pragma region Action Methods

    [[eosio::action]] void startchain(eosio::name owner, std::string publickey);
    [[eosio::action]] void configchain(eosio::name owner, blockbase::contractinfo infojson);
    [[eosio::action]] void startcandtime(eosio::name owner);
    [[eosio::action]] void secrettime(eosio::name owner);
    [[eosio::action]] void startsendtime(eosio::name owner);
    [[eosio::action]] void startrectime(eosio::name owner);
    [[eosio::action]] void prodtime(eosio::name owner);
    [[eosio::action]] void addcandidate(eosio::name owner, eosio::name candidate, uint64_t & worktimeinseconds, std::string & publickey, checksum256 secrethash);
    [[eosio::action]] void addsecret(eosio::name owner, eosio::name producer, checksum256 secret);
    [[eosio::action]] void rcandidate(eosio::name owner, eosio::name name);
    [[eosio::action]] void addencryptip(eosio::name owner, eosio::name name, std::vector<std::string> encryptedips);
    [[eosio::action]] void changecprod(eosio::name owner);
    [[eosio::action]] void addblock(eosio::name owner, eosio::name producer, blockbase::blockheaders block);
    [[eosio::action]] void exitrequest(eosio::name owner, eosio::name producer);
    [[eosio::action]] void blistremoval(eosio::name owner, eosio::name producer);
    [[eosio::action]] void iamready(eosio::name owner, eosio::name producer);
    [[eosio::action]] void resetreward(eosio::name sidechain, eosio::name claimer);
    [[eosio::action]] void verifyblock(eosio::name owner, eosio::name producer, std::string blockhash);
    [[eosio::action]] void endservice(eosio::name owner);

    bool comparenumbers(uint16_t variablenumber, uint16_t fixednumber);
    bool issecretvalid(eosio::name owner, eosio::name name, checksum256 secret);
    bool isblockprod(eosio::name owner, eosio::name producer);
    bool ispublickeyvalid(std::string publickey);
    bool isconfigvalid(blockbase::contractinfo info);
    bool iscandidatevalid(eosio::name owner, eosio::name producer, uint64_t worktimeinseconds);
    bool iscandidatetime(eosio::name owner);
    bool isblockvalid(eosio::name owner, blockheaders block);
    bool isprodtime(eosio::name owner, eosio::name producer);
    static bool isstakerecoverable(eosio::name contract, eosio::name owner, eosio::name producer);
    static bool isprod(eosio::name contract, eosio::name owner, eosio::name producer);
    void computation(eosio::name owner);
    void manageprod(eosio::name owner);
    void punishprod(eosio::name owner);
    void changewarning(eosio::name owner, eosio::name producer, uint16_t failedblocks, uint16_t producedblocks);
    void updatewarning(eosio::name owner, eosio::name producer, uint8_t warning);
    void enoughclientstake(eosio::name owner);
    void checkprodstake(eosio::name owner);
    void rewardprod(eosio::name owner, eosio::name producer, uint16_t quantity);
    void blockcount(eosio::name owner, eosio::name producer);
    void authassign(eosio::name owner, eosio::name contract, eosio::name permission1, uint8_t threshold);
    void linkauth(eosio::name contract, std::vector<eosio::name> actions, eosio::name permission);
    void nextcurrentprod(eosio::name owner, eosio::name nextproducer);
    void insertblock(eosio::name owner, eosio::name producer, blockbase::blockheaders block);
    void insertcandidate(eosio::name owner, eosio::name candidate, uint64_t & worktimeinseconds, std::string & publickey, checksum256 secrethash);
    void addprod(eosio::name owner, blockbase::candidates candidate);
    void addpublickey(eosio::name owner, eosio::name producer, std::string publickey);
    void eraseblockcount(eosio::name owner);
    void setenddate(eosio::name owner, uint8_t type);
    void startcount(eosio::name owner, bool computation = true);
    void infomanage(eosio::name owner, blockbase::contractinfo infojson);
    void deleteblockcount(eosio::name owner);
    void deleteprods(eosio::name owner);
    void deleteprods(eosio::name owner, std::vector<struct blockbase::producers> producers);
    void deleteips(eosio::name owner);
    void deleteips(eosio::name owner, std::vector<struct blockbase::producers> producers);
    void deletecprod(eosio::name owner, std::vector<struct producers> producerstoremove);
    void decisionmark(eosio::name owner);
    void changestate(struct blockbase::contractst states);
    uint8_t numberofips(float numberofproducers);
    uint8_t thresholdcalc(uint8_t producersnumber);
    std::vector<struct blockbase::candidates> choosecandidates(eosio::name owner);
    std::vector<struct blockbase::producers> checkbadprods(eosio::name owner);
    std::vector<struct blockbase::producers> checksendprods(eosio::name owner);
    static uint64_t getreward(eosio::name contract, eosio::name claimer);
    blockbase::producers getnextprod(eosio::name owner);
    std::vector<struct blockbase::blockheaders> getlastblock(eosio::name owner);
    std::vector<struct blockbase::producers> getreadyprods(eosio::name owner);
    std::map<eosio::name, asset> static getbadprods(const name &contract, const name &owner);
#pragma endregion
};