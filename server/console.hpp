#ifndef CONSOLE_HPP
#define CONSOLE_HPP

#include <pthread.h>
#include <iostream>
#include <string>
#include <cstring>
#include "gameserver.hpp"

class ServerConsole
{
    public:
        ServerConsole();
        void stop(void);
        bool is_running(void);
    private:
        pthread_t thread;
};


void * console_logic(void *)
{
    std::cout << "Started server console..." << std::endl;

    std::string input_string;

    while(game_is_running)
    {
        if(std::getline(std::cin, input_string))
        {
            if(input_string == "stop")
            {
                game_is_running = false;
            }
            else if (input_string.length() > 2 && input_string.substr(0,3) == "say")
            {
                if(input_string.length() > 4)
                {
                    gameserver::BroadcastMessage("Server: " + input_string.substr(4));
                }
                else
                {
                    std::cout << "Must pass a valid message!" << std::endl;
                }
                
            }
            else
            {
                std::cout << std::endl
                << "Invalid console command!" << std::endl
                << "Valid commands are:" << std::endl
                << "stop" << std::endl
                << "say [message]" << std::endl;
            }
        }
    }
    return 0;
}

ServerConsole::ServerConsole(void)
{
    //Intialized values
    game_is_running = true;

    thread = pthread_t();

    pthread_create(&(this->thread), NULL, console_logic, NULL);
    
}

void ServerConsole::stop(void)
{
    if(this->thread != 0)
    {
        pthread_join(this->thread, NULL);
    }
}

bool ServerConsole::is_running(void)
{
    return game_is_running;
}

#endif