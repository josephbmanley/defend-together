#ifndef GAMESERVER_HPP
#define GAMESERVER_HPP

#include <enet/enet.h>
#include <iostream>
#include <string>
#include <sstream>
#include <cstring>

#include <unistd.h>
#include <pthread.h>

#include "gamemap.hpp"
using namespace std;

bool game_is_running = false;

const int SERVER_PORT = 7777;
const int MAX_PLAYERS = 32;

const int EVENT_RATE = 1000;

//Game Data
static GameMap gamemap(256,256);
static std::string usernames[MAX_PLAYERS];

static ENetHost* server;

//===================
// Game Server Logic
//===================
namespace gameserver
{
    void Intialize()
    {
        printf("Starting DT game server...\n");

        for(int i = 0; i < MAX_PLAYERS; i++)
        {
            usernames[i] = "";
        }
    }

    void PlayerMove(ENetEvent* event)
    {
        int peer_id = event->peer -> incomingPeerID;

        float move_x = 0;
        float move_y = 0;

        //Remove id marker from packet data
        std::string data_input((char*)event->packet->data);
        data_input = data_input.substr(2);
        
        //Parse input string
        std::stringstream ss(data_input);
        std::string tempString;
        std::getline(ss, tempString, ',');
        move_x = std::stof(tempString);
        std::getline(ss, tempString, '\n');
        move_y = std::stof(tempString);

        //Update player position
        std::string resp = "2|" + gamemap.move_enitity_relatively(usernames[peer_id], "player", move_x, move_y);
        const char* data = resp.c_str();
        ENetPacket* packet = enet_packet_create(data, strlen(data) + 1, 0);
        enet_host_broadcast(server, 0, packet);
    }

    void DataRequest(ENetEvent* event)
    {
        //Create world data dump
        std::string map_dump_resp("2|");
        map_dump_resp += gamemap.get_tile_dump() + gamemap.get_entity_dump();
        const char* data = map_dump_resp.c_str();

        //Build and send reliable packet to requester
        ENetPacket* packet = enet_packet_create(data, strlen(data) + 1, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(event->peer, 0, packet);
    }

    void Authenticate(ENetEvent* event)
    {
        //Grab username from packet data
        std::string username((char*)event->packet->data);
        username = username.substr(2);

        //Determine if peer already has username
        int peer_id = event -> peer -> incomingPeerID;
        const char* data;
        if(usernames[peer_id] == "")
        {
            //Update username array
            usernames[peer_id] = username;

            //Spawn entity
            std::string spawn_data = gamemap.spawn_entity(usernames[peer_id], "player", 128, 129);

            //Tell peers about new player
            data = ("2|" + spawn_data).c_str();
            ENetPacket* packet = enet_packet_create(data, strlen(data) + 1, ENET_PACKET_FLAG_RELIABLE);
            enet_host_broadcast(server, 0, packet);

            std::cout << "'" << usernames[peer_id] << "' spawned!" << std::endl;

            std::string okay_response("1|OK");
            data = (okay_response + username).c_str();
        
            //Return success
            packet = enet_packet_create(data, strlen(data) + 1, ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(event -> peer, 0, packet);
        }
        else
        {
            std::string resp("1|Already logged in!");
            data = resp.c_str();
        
            //Return reliable error message
            ENetPacket* packet = enet_packet_create(data, strlen(data) + 1, ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(event -> peer, 0, packet);
        }
    }

    void SendPlayerMessage(ENetPeer* peer, std::string message)
    {
        const char* data = message.c_str();

        ENetPacket* packet = enet_packet_create(data, strlen(data) + 1, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(peer, 1, packet);
    }

    bool SendPlayerMessage(std::string username, std::string message)
    {
        for(int i = 0; i < MAX_PLAYERS; i++)
        {
            if(usernames[i] == username)
            {
                SendPlayerMessage(&(server->peers[i]), message);
                return true;
            }
        }
        return false;
    }

    void BroadcastMessage(std::string message)
    {
        const char* data = message.c_str();

        std::cout << data << std::endl;

        ENetPacket* packet = enet_packet_create(data, strlen(data) + 1, ENET_PACKET_FLAG_RELIABLE);
        enet_host_broadcast(server, 1, packet);
    }

    void ProcessChatMessage(ENetEvent* event)
    {
        int peer_id = event->peer -> incomingPeerID;
        if(usernames[peer_id] != "")
        {
            //Parse input string
            std::stringstream ss((char*)event->packet->data);
            std::string chat_message;
            std::getline(ss, chat_message, '\n');
            std::string resp = "<" + usernames[peer_id] + "> " + chat_message;
            BroadcastMessage(resp);

        }
    }
    
    void HandlePlayerDisconnect(ENetEvent* event)
    {
        //Clear username data and remove entity
        std::string username = usernames[event -> peer -> incomingPeerID];
        if(username != "")
        {
            std::cout << "'" << username << "' disconnected" << std::endl;
            gamemap.remove_entity(username,"player");
            usernames[event -> peer -> incomingPeerID] = "";

            std::string resp = "2|delete,player:" + username;
            const char* data = resp.c_str();
            ENetPacket* packet = enet_packet_create(data, strlen(data) + 1, ENET_PACKET_FLAG_RELIABLE);
            enet_host_broadcast(server, 0, packet);
        }
    }

    void ProcessGeneralInput(ENetEvent* event)
    {
        char c [1];
        c[0] = event->packet->data[0];
        switch (c[0])
        {
        case '1':
            gameserver::Authenticate(event);
            break;
        case '2':
            gameserver::DataRequest(event);
            break;
        case '3':
            gameserver::PlayerMove(event);
            break;
        default:
            std::cout << "Invalid packet recieved!" <<std::endl;
            break;
        }
    }

    pthread_t ticker_thread;

    void * GameTicker(void *)
    {
        while(game_is_running)
        {
            usleep(50000*3);
            std::string data_string = "2|"+gamemap.world_tick();
            const char* data = data_string.c_str();
            ENetPacket* packet = enet_packet_create(data, strlen(data), ENET_PACKET_FLAG_UNSEQUENCED);
            enet_host_broadcast(server, 0, packet);
        }
        return 0;
    }

    void StartTicker()
    {
        pthread_create(&ticker_thread, NULL, GameTicker, NULL);
    }
}

#endif