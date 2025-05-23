/*! \file sc2_gametypes.h
    \brief Types used in setting up a game.
*/
#pragma once

#include <string>
#include <vector>

#include "sc2_common.h"

namespace sc2 {

typedef uint64_t Tag;
static const Tag NullTag = 0LL;

enum Race { Terran, Zerg, Protoss, Random };

enum GameResult { Win, Loss, Tie, Undecided };

enum Difficulty {
    VeryEasy = 1,
    Easy = 2,
    Medium = 3,
    MediumHard = 4,
    Hard = 5,
    HardVeryHard = 6,
    VeryHard = 7,
    CheatVision = 8,
    CheatMoney = 9,
    CheatInsane = 10
};

enum PlayerType { Participant = 1, Computer = 2, Observer = 3 };

enum AIBuild { RandomBuild = 1, Rush = 2, Timing = 3, Power = 4, Macro = 5, Air = 6 };

enum class ChatChannel { All = 0, Team = 1 };

class Agent;

//! Setup for a player in a game.
struct PlayerSetup {
    //! Player can be a Participant (usually an agent), Computer (in-built AI) or Observer.
    PlayerType type;
    //! Agent, if one is available.
    Agent* agent;
    //! Name of this player.
    std::string player_name;

    // Only used for Computer

    //! Race: Terran, Zerg or Protoss. Only for playing against the built-in AI.
    Race race;
    //! Difficulty: Only for playing against the built-in AI.
    Difficulty difficulty;
    //! Build type, used by computer opponent.
    AIBuild ai_build;

    PlayerSetup() : type(Participant), agent(nullptr), race(Terran), difficulty(Easy), ai_build(RandomBuild) {};

    PlayerSetup(PlayerType in_type, Race in_race, Agent* in_agent = nullptr, const std::string& in_player_name = "",
                Difficulty in_difficulty = Easy, AIBuild in_ai_build = RandomBuild)
        : type(in_type),
          agent(in_agent),
          player_name(in_player_name),
          race(in_race),
          difficulty(in_difficulty),
          ai_build(in_ai_build) {
    }
};

static inline PlayerSetup CreateParticipant(Race race, Agent* agent, const std::string& player_name = "") {
    return PlayerSetup(PlayerType::Participant, race, agent, player_name);
}

static inline PlayerSetup CreateComputer(Race race, Difficulty difficulty = Easy, AIBuild ai_build = RandomBuild,
                                         const std::string& player_name = "") {
    return PlayerSetup(PlayerType::Computer, race, nullptr, player_name, difficulty, ai_build);
}

//! Port setup for a client.
struct PortSet {
    int game_port;
    int base_port;

    PortSet() : game_port(-1), base_port(-1) {
    }

    bool IsValid() const {
        return game_port > 0 && base_port > 0;
    }
};

//! Port setup for one or more clients in a game.
struct Ports {
    PortSet server_ports;
    std::vector<PortSet> client_ports;
    int shared_port;

    Ports() : shared_port(-1) {
    }

    bool IsValid() const {
        if (shared_port < 1)
            return false;
        if (!server_ports.IsValid())
            return false;
        if (client_ports.size() < 1)
            return false;
        for (std::size_t i = 0; i < client_ports.size(); ++i)
            if (!client_ports[i].IsValid())
                return false;

        return true;
    }
};

static const int max_path_size = 512;
static const int max_version_size = 32;
static const int max_num_players = 16;

//! Information about a player in a replay.
struct ReplayPlayerInfo {
    //! Player ID.
    int player_id;
    //! Player ranking.
    int mmr;
    //! Player actions per minute.
    int apm;
    //! Actual player race.
    Race race;
    //! Selected player race. If the race is "Random", the race data member may be different.
    Race race_selected;
    //! If the player won or lost.
    GameResult game_result;

    ReplayPlayerInfo() : player_id(0), mmr(-10000), apm(0), race(Random), race_selected(Random) {
    }
};

//! Information about a replay file.
struct ReplayInfo {
    float duration;
    unsigned int duration_gameloops;
    int32_t num_players;
    uint32_t data_build;
    uint32_t base_build;
    std::string map_name;
    std::string map_path;
    std::string replay_path;
    std::string version;
    std::string data_version;
    ReplayPlayerInfo players[max_num_players];

    ReplayInfo() : duration(0.0f), duration_gameloops(0), num_players(0), data_build(0), base_build(0) {
    }

    bool GetPlayerInfo(ReplayPlayerInfo& replay_player_info, int playerID) const {
        for (int i = 0; i < num_players; ++i) {
            if (playerID == players[i].player_id) {
                replay_player_info = players[i];
                return true;
            }
        }

        return false;
    }

    float GetGameloopsPerSecond() const {
        return float(duration_gameloops) / duration;
    }
};

struct PlayerResult {
    PlayerResult(uint32_t player_id, GameResult result) : player_id(player_id), result(result) {};

    uint32_t player_id;
    GameResult result;
};

struct ChatMessage {
    uint32_t player_id;
    std::string message;
};

}  // namespace sc2
