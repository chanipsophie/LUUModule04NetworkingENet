#include <enet/enet.h>
#include <string>
#include <iostream>
#include <thread>
#include <stdlib.h>

using namespace std;

ENetHost* NetHost = nullptr;
ENetPeer* Peer = nullptr;
bool IsServer = false;
thread* PacketThread = nullptr;
constexpr int NUMBER_OF_PLAYERS = 2;
int CurrentNumPlayers = 0;
int RandomNumber = rand() % 10; // 0-9
//int RandomNumber = 1; // 0-9

enum PacketHeaderTypes
{
    PHT_Invalid = 0,
    PHT_IsDead,
    PHT_Position,
    PHT_Guess,
    PHT_Count,
    PHT_Start
};

class PlayerInfo
{
    int connectionId = 0;
    string name = {};
    int numGuesses = 0;

};

struct GamePacket
{
    GamePacket() {}
    PacketHeaderTypes Type = PHT_Invalid;
};

struct IsDeadPacket : public GamePacket
{
    IsDeadPacket()
    {
        Type = PHT_IsDead;
    }

    int playerId = 0;
    bool IsDead = false;
};


class NumGuessPacket : public GamePacket
{
public:
    NumGuessPacket()
    {
        Type = PHT_Guess;
    }
    int Guess;
};

class GameStartPacket : public GamePacket
{
public:
    GameStartPacket()
    {
        Type = PHT_Start;
    }
};


struct PositionPacket : public GamePacket
{
    PositionPacket()
    {
        Type = PHT_Position;
    }

    int playerId = 0;
    int x = 0;
    int y = 0;
};

//can pass in a peer connection if wanting to limit
bool CreateServer()
{
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 1234;
    NetHost = enet_host_create(&address /* the address to bind the server host to */,
        32      /* allow up to 32 clients and/or outgoing connections */,
        2      /* allow up to 2 channels to be used, 0 and 1 */,
        0      /* assume any amount of incoming bandwidth */,
        0      /* assume any amount of outgoing bandwidth */);

    return NetHost != nullptr;
}

bool CreateClient()
{
    NetHost = enet_host_create(NULL /* create a client host */,
        1 /* only allow 1 outgoing connection */,
        2 /* allow up 2 channels to be used, 0 and 1 */,
        0 /* assume any amount of incoming bandwidth */,
        0 /* assume any amount of outgoing bandwidth */);

    return NetHost != nullptr;
}

bool AttemptConnectToServer()
{
    ENetAddress address;
    /* Connect to some.server.net:1234. */
    enet_address_set_host(&address, "127.0.0.1");
    address.port = 1234;
    /* Initiate the connection, allocating the two channels 0 and 1. */
    Peer = enet_host_connect(NetHost, &address, 2, 0);
    return Peer != nullptr;
}

void SentGuessNumberPacket(int GuessNumber, const ENetEvent& event)
{
    auto GuessNumberPacket = make_unique<NumGuessPacket>(); //client sent guessNumber to server 
    GuessNumberPacket->Guess = GuessNumber;

    ENetPacket* packet = enet_packet_create(GuessNumberPacket.get(),
        sizeof(GuessNumberPacket.get()),
        ENET_PACKET_FLAG_RELIABLE);
    /* One could also broadcast the packet by         */
    //enet_host_broadcast(NetHost, 0, packet);
    enet_peer_send(event.peer, 0, packet);

    /* One could just use enet_host_service() instead. */
    //enet_host_service();
    //enet_host_flush(NetHost);
}


void HandleReceivePacket(const ENetEvent& event) //Both client and server recieve packet logic
{
    GamePacket* RecGamePacket = (GamePacket*)(event.packet->data);
    if (RecGamePacket)
    {
        cout << "Received Game Packet " << endl;

        if (RecGamePacket->Type == PHT_IsDead)
        {
            cout << "u dead?" << endl;
            IsDeadPacket* DeadGamePacket = (IsDeadPacket*)(event.packet->data);
            if (DeadGamePacket)
            {
                string response = (DeadGamePacket->IsDead ? "yeah" : "no");
                cout << response << endl;
            }
        }
        else if (RecGamePacket->Type == PHT_Start)
        {
            cout << "Please guess a Number!" << endl;
            int Number;
            cin >> Number;
            SentGuessNumberPacket(Number, event);
        }
        else if (RecGamePacket->Type == PHT_Guess)
        {
            NumGuessPacket* IsSameNumberPacket = (NumGuessPacket*)(event.packet->data);
            if (IsSameNumberPacket->Guess == RandomNumber)
            {
                string msg = "You Won !";
                ENetPacket* packet = enet_packet_create(msg.c_str(),
                    strlen(msg.c_str()) + 1,
                    ENET_PACKET_FLAG_RELIABLE);
                /* One could also broadcast the packet by         */
                //enet_host_broadcast(NetHost, 0, packet);
                enet_peer_send(event.peer, 0, packet);
            }
            else
            {
                string msg = "Try Again !";
                ENetPacket* packet = enet_packet_create(msg.c_str(),
                    strlen(msg.c_str()) + 1,
                    ENET_PACKET_FLAG_RELIABLE);
                /* One could also broadcast the packet by         */
                //enet_host_broadcast(NetHost, 0, packet);
                enet_peer_send(event.peer, 0, packet);
            }
        }
    }
    else
    {
        cout << "Invalid Packet " << endl;
    }

    /* Clean up the packet now that we're done using it. */
    enet_packet_destroy(event.packet);
    {
        enet_host_flush(NetHost);
    }
}

void BroadcastGameStartPacket()
{
    auto IsGameStartPacket = make_unique<GameStartPacket>();
    ENetPacket* packet = enet_packet_create(IsGameStartPacket.get(),
        sizeof(IsGameStartPacket.get()),
        ENET_PACKET_FLAG_RELIABLE);
    /* One could also broadcast the packet by         */
    enet_host_broadcast(NetHost, 0, packet);
    //enet_peer_send(event.peer, 0, packet);

    /* One could just use enet_host_service() instead. */
    //enet_host_service();
    enet_host_flush(NetHost);
}

void BroadcastIsDeadPacket()
{
    IsDeadPacket* DeadPacket = new IsDeadPacket();
    DeadPacket->IsDead = true;
    ENetPacket* packet = enet_packet_create(DeadPacket,
        sizeof(IsDeadPacket),
        ENET_PACKET_FLAG_RELIABLE);

    /* One could also broadcast the packet by         */
    enet_host_broadcast(NetHost, 0, packet);
    //enet_peer_send(event.peer, 0, packet);

    /* One could just use enet_host_service() instead. */
    //enet_host_service();
    enet_host_flush(NetHost);
    delete DeadPacket;
}

void ServerProcessPackets()
{
    while (1)
    {
        ENetEvent event;
        while (enet_host_service(NetHost, &event, 1000) > 0)
        {
            switch (event.type)
            {
            case ENET_EVENT_TYPE_CONNECT:
                cout << "A new client connected from "
                    << event.peer->address.host
                    << ":" << event.peer->address.port
                    << endl;
                /* Store any relevant client information here. */
                event.peer->data = (void*)("Client information");
                BroadcastIsDeadPacket();

                CurrentNumPlayers++;
                if (CurrentNumPlayers == NUMBER_OF_PLAYERS) 
                {
                    string msg = "We have enough players, let's start the game !";

                    ENetPacket* packet = enet_packet_create(msg.c_str(),
                        strlen(msg.c_str()) + 1,
                        ENET_PACKET_FLAG_RELIABLE);
                    enet_host_broadcast(NetHost, 0, packet);
                    enet_host_flush(NetHost);

                    BroadcastGameStartPacket();
                }
                else
                {
                    string msg = "hold tight! waiting for other player to join.";

                    ENetPacket* packet = enet_packet_create(msg.c_str(),
                        strlen(msg.c_str()) + 1,
                        ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(event.peer, 0, packet);
                    enet_host_flush(NetHost);
                }

                break;
            case ENET_EVENT_TYPE_RECEIVE:
                HandleReceivePacket(event);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                cout << (char*)event.peer->data << "disconnected." << endl;
                /* Reset the peer's client information. */
                event.peer->data = NULL;
                //notify remaining player that the game is done due to player leaving
            }
        }
    }
}

void ClientProcessPackets()
{
    while (1)
    {
        ENetEvent event;
        /* Wait up to 1000 milliseconds for an event. */
        while (enet_host_service(NetHost, &event, 10000) > 0)
        {
            switch (event.type)
            {
            case  ENET_EVENT_TYPE_CONNECT:
                cout << "Connection succeeded " << endl;
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                cout << (char*)event.packet->data << endl;
                HandleReceivePacket(event);
                break;
            }
        }
    }
}

int main(int argc, char** argv)
{
    if (enet_initialize() != 0)
    {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        cout << "An error occurred while initializing ENet." << endl;
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);

    cout << "1) Create Server " << endl;
    cout << "2) Create Client " << endl;
    int UserInput;
    cin >> UserInput;
    if (UserInput == 1)
    {
        //How many players?

        if (!CreateServer())
        {
            fprintf(stderr,
                "An error occurred while trying to create an ENet server.\n");
            exit(EXIT_FAILURE);
        }

        IsServer = true;
        cout << "waiting for players to join..." << endl;
        PacketThread = new thread(ServerProcessPackets);
    }
    else if (UserInput == 2)
    {
        if (!CreateClient())
        {
            fprintf(stderr,
                "An error occurred while trying to create an ENet client host.\n");
            exit(EXIT_FAILURE);
        }

        ENetEvent event;
        if (!AttemptConnectToServer())
        {
            fprintf(stderr,
                "No available peers for initiating an ENet connection.\n");
            exit(EXIT_FAILURE);
        }

        PacketThread = new thread(ClientProcessPackets);

        //handle possible connection failure
        {
            //enet_peer_reset(Peer);
            //cout << "Connection to 127.0.0.1:1234 failed." << endl;
        }
    }
    else
    {
        cout << "Invalid Input" << endl;
    }


    if (PacketThread)
    {
        PacketThread->join();
    }
    delete PacketThread;
    if (NetHost != nullptr)
    {
        enet_host_destroy(NetHost);
    }

    return EXIT_SUCCESS;
}

void Draw()
{

}

void Test()
{

}