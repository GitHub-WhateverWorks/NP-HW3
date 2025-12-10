#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

enum class PacketType {

    // Developer actions
    DEV_REGISTER = 1,
    DEV_LOGIN,
    DEV_UPLOAD_GAME,
    DEV_UPDATE_GAME,
    DEV_REMOVE_GAME,
    DEV_LIST_MY_GAMES,

    // Player/Lobby actions
    PLAYER_REGISTER = 100,
    PLAYER_LOGIN,
    PLAYER_LIST_GAMES,
    PLAYER_DOWNLOAD_GAME,
    PLAYER_CREATE_ROOM,
    PLAYER_JOIN_ROOM,
    PLAYER_START_GAME,
    PLAYER_SUBMIT_REVIEW = 140,
    PLAYER_GET_REVIEWS  = 141,
    // Game server <-> Game client
    JOIN_GAME = 200,
    PLAYER_ACTION,
    STATE_UPDATE,
    GAME_END,

    // Generic
    SERVER_RESPONSE = 300,
    ERROR_RESPONSE,
    KEEPALIVE = 99

};

#endif
