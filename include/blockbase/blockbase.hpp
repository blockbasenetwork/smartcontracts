#include <eosio/binary_extension.hpp>

using namespace eosio;

static const eosio::name BLOCKBASE_TOKEN = eosio::name("blockbasetkn");
static const std::string BLOCKBASE_TOKEN_SYMBOL = "BBT";

class[[eosio::contract]] blockbase : public eosio::contract {

  public:
    blockbase(eosio::name receiver, eosio::name code, eosio::datastream<const char *> ds) : eosio::contract(receiver, code, ds) {}

    // Minimum
    const uint32_t MIN_PAYMENT = 0;
    const uint32_t MIN_CANDIDATE_STAKE = 1;
    const uint32_t MIN_REQUESTER_STAKE = 1;
    const uint16_t MIN_CANDIDATURE_TIME_IN_SECONDS = 1;
    const uint16_t MIN_IP_SEND_TIME_IN_SECONDS = 1;
    const uint8_t MIN_REQUIRED_PRODUCERS = 1;

    // Warning
    const uint8_t WARNING_TYPE_CLEAR = 0;
    const uint8_t WARNING_TYPE_FLAGGED = 1;
    const uint8_t WARNING_TYPE_PUNISH = 2;

    // Producer Types
    const uint8_t PRODUCER_TYPE_VALIDATOR = 1;
    const uint8_t PRODUCER_TYPE_HISTORY = 2;
    const uint8_t PRODUCER_TYPE_FULL = 3;

    // Contract Info
    const double MIN_PRODUCERS_IN_CHAIN_THRESHOLD = 0.40;
    const double MIN_PRODUCERS_TO_PRODUCE_THRESHOLD = 0.70;
    const double MIN_BLOCKS_THRESHOLD_FOR_PUNISH = 0.80;

    //TODO - change to enum
    const uint8_t CHANGE_PRODUCER_ID = 0;
    const uint8_t CANDIDATURE_TIME_ID = 1;
    const uint8_t SECRET_TIME_ID = 2;
    const uint8_t SEND_TIME_ID = 3;
    const uint8_t RECEIVE_TIME_ID = 4;
    const uint8_t PRODUCTION_TIME_ID = 5;

    const eosio::name VERIFY_PERMISSION_NAME = eosio::name("verifyblock");
    const std::vector<eosio::name> VERIFY_PERMISSION_ACTION{eosio::name("verifyblock")};
    const eosio::name CKEY = eosio::name("currentprod");

    // Producers Table
    struct [[eosio::table]] producers {
        eosio::name key;
        std::string public_key;
        uint8_t producer_type;
        uint8_t warning_type;
        uint64_t work_duration_in_seconds;
        uint64_t sidechain_start_date_in_seconds;
        bool is_ready_to_produce;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("producers"), producers> producersIndex;

    // Candidates Table
    struct [[eosio::table]] candidates {
        eosio::name key;
        std::string public_key;
        uint8_t producer_type;
        checksum256 secret_hash;
        checksum256 secret;
        uint64_t work_duration_in_seconds;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("candidates"), candidates> candidatesIndex;

    // Reserved Seats Table
    struct [[eosio::table]] reservedseat {
        eosio::name key;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("reservedseat"), reservedseat> reservedseatIndex;

    // Blockheaders Table
    struct [[eosio::table]] blockheaders {
        std::string producer;
        std::string block_hash;
        std::string previous_block_hash;
        uint64_t last_trx_sequence_number;
        uint64_t sequence_number;
        uint64_t timestamp;
        uint64_t transactions_count;
        std::string producer_signature;
        std::string merkletree_root_hash;
        bool is_verified;
        bool is_latest_block;
        uint64_t block_size_in_bytes;
        uint64_t primary_key() const { return sequence_number; }
    };
    typedef eosio::multi_index<eosio::name("blockheaders"), blockheaders> blockheadersIndex;

    // Client Table
    struct [[eosio::table]] client {
        eosio::name key;
        std::string public_key;
        eosio::binary_extension<uint64_t> sidechain_creation_timestamp;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("client"), client> clientIndex;

    // Ip addresses Table
    struct [[eosio::table]] ipaddress {
        eosio::name key;
        std::string public_key;
        std::vector<std::string> encrypted_ips;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("ipaddress"), ipaddress> ipsIndex;

    // Contract State Table
    struct [[eosio::table]] contractst {
        eosio::name key;
        bool has_chain_started;
        bool is_configuration_phase;
        bool is_candidature_phase;
        bool is_secret_sending_phase;
        bool is_ip_sending_phase;
        bool is_ip_retrieving_phase;
        bool is_production_phase;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("contractst"), contractst> stateIndex;

    // Contract Information Table
    struct [[eosio::table]] contractinfo {
        eosio::name key;
        uint64_t max_payment_per_block_validator_producers;
        uint64_t max_payment_per_block_history_producers;
        uint64_t max_payment_per_block_full_producers;
        uint64_t min_payment_per_block_validator_producers;
        uint64_t min_payment_per_block_history_producers;
        uint64_t min_payment_per_block_full_producers;
        uint64_t min_candidature_stake;
        uint32_t number_of_validator_producers_required;
        uint32_t number_of_history_producers_required;
        uint32_t number_of_full_producers_required;
        uint32_t candidature_phase_duration_in_seconds;
        uint32_t secret_sending_phase_duration_in_seconds;
        uint32_t ip_sending_phase_duration_in_seconds;
        uint32_t ip_retrieval_phase_duration_in_seconds;
        uint32_t candidature_phase_end_date_in_seconds;
        uint32_t secret_sending_phase_end_date_in_seconds;
        uint32_t ip_sending_phase_end_date_in_seconds;
        uint32_t ip_retrieval_phase_end_date_in_seconds;
        uint32_t block_time_in_seconds;
        uint32_t num_blocks_between_settlements;
        uint64_t block_size_in_bytes;
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
        uint8_t num_blocks_failed;
        uint8_t num_blocks_produced;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("blockscount"), blockscount> blockscountIndex;

    // Current Producer Table
    struct [[eosio::table]] currentprod {
        eosio::name key;
        eosio::name producer;
        uint64_t production_start_date_in_seconds;
        bool has_produced_block;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("currentprod"), currentprod> currentprodIndex;

    // Rewards Producer Table
    struct [[eosio::table]] pendingrewards {
        eosio::name key;
        uint64_t reward;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("rewards"), pendingrewards> rewardsIndex;

    // History Validation
    struct [[eosio::table]] histval {
        eosio::name key;
        std::string block_hash;
        std::vector<std::string> verify_signatures;
        std::vector<char> packed_transaction;
        std::string block_byte_in_hex;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("histval"), histval> histvalIndex;

    // AccountsPermissions
    struct [[eosio::table]] accperm {
        eosio::name key;
        std::string public_key;
        std::string permissions;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("accperm"), accperm> accpermIndex;

    // VerifySignatures
    struct [[eosio::table]] verifysig {
        eosio::name key;
        std::string block_hash;
        std::string verify_signature;
        std::vector<char> packed_transaction;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("verifysig"), verifysig> verifysigIndex;

    // Version Table
    struct [[eosio::table]] version {
        eosio::name key;
        uint32_t software_version;
        uint64_t primary_key() const { return key.value; }
    };
    typedef eosio::multi_index<eosio::name("version"), version> versionIndex;

    [[eosio::action]] void startchain(eosio::name owner, std::string publicKey);
    [[eosio::action]] void configchain(eosio::name owner, blockbase::contractinfo infoJson, std::vector<eosio::name> reservedSeats, uint32_t softwareVersion);
    [[eosio::action]] void startcandtime(eosio::name owner);
    [[eosio::action]] void secrettime(eosio::name owner);
    [[eosio::action]] void startsendtime(eosio::name owner);
    [[eosio::action]] void startrectime(eosio::name owner);
    [[eosio::action]] void startprodtime(eosio::name owner);
    [[eosio::action]] void addcandidate(eosio::name owner, eosio::name candidate, std::string & publicKey, checksum256 secretHash, uint8_t producerType, uint32_t softwareVersion);
    [[eosio::action]] void addsecret(eosio::name owner, eosio::name producer, checksum256 secret);
    [[eosio::action]] void rcandidate(eosio::name owner, eosio::name name);
    [[eosio::action]] void addencryptip(eosio::name owner, eosio::name name, std::vector<std::string> encryptedIps);
    [[eosio::action]] void changecprod(eosio::name owner);
    [[eosio::action]] void addblock(eosio::name owner, eosio::name producer, blockbase::blockheaders block);
    [[eosio::action]] void removeblisted(eosio::name owner, eosio::name producer);
    [[eosio::action]] void stopproducing(eosio::name owner, eosio::name producer);
    [[eosio::action]] void iamready(eosio::name owner, eosio::name producer);
    [[eosio::action]] void resetreward(eosio::name sidechain, eosio::name claimer);
    [[eosio::action]] void verifyblock(eosio::name owner, eosio::name producer, std::string blockHash);
    [[eosio::action]] void endservice(eosio::name owner);
    [[eosio::action]] void blacklistprod(eosio::name owner);
    [[eosio::action]] void reqhistval(eosio::name owner, eosio::name producer, std::string blockHash);
    [[eosio::action]] void addblckbyte(eosio::name owner, eosio::name producer, std::string byteInHex);
    [[eosio::action]] void histvalidate(eosio::name owner, eosio::name producer);
    [[eosio::action]] void addaccperm(eosio::name owner, eosio::name account, std::string publicKey, std::string permissions);
    [[eosio::action]] void remaccperm(eosio::name owner, eosio::name account);
    [[eosio::action]] void addversig(eosio::name owner, eosio::name account, std::string blockHash, std::string verifySignature, std::vector<char> packedTransaction);
    [[eosio::action]] void exitrequest(eosio::name owner, eosio::name account);

    std::map<eosio::name, asset> static GetProducersToPunishInfo(const name &contract, const name &owner);
    static uint64_t GetProducerRewardAmount(eosio::name contract, eosio::name claimer);
    static bool IsProducer(eosio::name contract, eosio::name owner, eosio::name producer);
    static bool IsStakeRecoverable(eosio::name contract, eosio::name owner, eosio::name producer);
    static bool IsServiceRequester(const name &contract, const name &owner);

  private:
    bool IsSecretValid(eosio::name owner, eosio::name name, checksum256 secret);
    bool HasBlockBeenProduced(eosio::name owner, eosio::name producer);
    bool IsPublicKeyValid(eosio::name owner, std::string publicKey);
    bool IsConfigurationValid(blockbase::contractinfo info);
    bool IsCandidateValid(eosio::name owner, eosio::name producer);
    bool IsCandidaturePhase(eosio::name owner);
    bool IsTimestampValid(eosio::name owner, blockheaders block);
    bool IsBlockSizeValid(eosio::name owner, blockheaders block);
    bool IsPreviousBlockHashAndSequenceNumberValid(eosio::name owner, blockheaders block);
    bool IsProducerTurn(eosio::name owner, eosio::name producer);
    bool IsVersionValid(eosio::name owner, uint32_t softwareVersion);
    void RunSettlement(eosio::name owner);
    void RemoveBadProducers(eosio::name owner);
    void EvaluateProducer(eosio::name owner, eosio::name producer, uint16_t failedBlocks, uint16_t producedBlocks);
    void UpdateWarningDAM(eosio::name owner, eosio::name producer, uint8_t warningType);
    void IsRequesterStakeEnough(eosio::name owner);
    void RewardProducerDAM(eosio::name owner, eosio::name producer, uint64_t quantity);
    void UpdateBlockCount(eosio::name owner, eosio::name producer);
    void UpdateCurrentProducerDAM(eosio::name owner, eosio::name nextProducer);
    void AddCandidatesWithReservedSeat(eosio::name owner);
    void AddBlockDAM(eosio::name owner, eosio::name producer, blockbase::blockheaders block);
    void AddCandidateDAM(eosio::name owner, eosio::name candidate, std::string & publicKey, checksum256 secretHash, uint8_t producerType);
    void AddProducerDAM(eosio::name owner, blockbase::candidates candidate);
    void AddPublicKeyDAM(eosio::name owner, eosio::name producer, std::string publicKey);
    void RemoveBlockCountDAM(eosio::name owner, std::vector<struct producers> producers);
    void SetEndDateDAM(eosio::name owner, uint8_t type);
    void ResetBlockCountDAM(eosio::name owner);
    void UpdateContractInfoDAM(eosio::name owner, blockbase::contractinfo infoJson);
    void RemoveBlockCountDAM(eosio::name owner);
    void RemoveProducersDAM(eosio::name owner);
    void RemoveProducersDAM(eosio::name owner, std::vector<struct blockbase::producers> producers);
    void RemoveIPsDAM(eosio::name owner);
    void RemoveIPsDAM(eosio::name owner, std::vector<struct blockbase::producers> producers);
    void DeleteCurrentProducerDAM(eosio::name owner, std::vector<struct producers> producersToRemove);
    void ReOpenCandidaturePhaseIfRequired(eosio::name owner);
    void ChangeContractStateDAM(struct blockbase::contractst states);
    void RemoveCandidateDAM(eosio::name owner, eosio::name candidate);
    void SoftwareVersionDAM(eosio::name owner, uint32_t softwareVersion);
    uint8_t CalculateNumberOfIPsRequired(float numberOfProducers);
    uint8_t CalculateMultiSigThreshold(uint8_t producersNumber);
    uint64_t CalculateRewardBasedOnBlockSize(eosio::name owner, struct blockbase::producers producer);
    std::vector<struct blockbase::candidates> RunCandidatesSelection(eosio::name owner);
    std::vector<struct blockbase::candidates> RunCandidatesSelectionForType(eosio::name owner, uint8_t producerType);
    std::vector<struct blockbase::producers> GetPunishedProducers(eosio::name owner);
    std::vector<struct blockbase::producers> GetProducersWhoFailedToSendIPs(eosio::name owner);
    blockbase::producers GetNextProducer(eosio::name owner);
    std::vector<struct blockbase::blockheaders> GetLatestBlock(eosio::name owner);
    std::vector<struct blockbase::producers> GetReadyProducers(eosio::name owner);
    void RemoveProducerWithWorktimeFinnished(eosio::name owner);
    void CheckHistoryValidation(eosio::name owner);
};