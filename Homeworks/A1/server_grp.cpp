#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <algorithm>
#include <fstream>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10
#define PORT 12345

// Mutex for thread-safe console output and client management
std::mutex cout_mutex;
std::mutex clients_mutex;


std :: unordered_map < int , std :: string > clients ; // Clientsocket -> username
std :: unordered_map < std :: string , int> sock ; // Username -> Clientsocket -> 
std :: unordered_map < std :: string , std :: string > user_credentials ; //Username -> password
std :: unordered_map < std :: string , std :: unordered_set < int > >groups ; // Group -> client sockets


//Load users from users.txt file
void load_users_from_file(const std::string& filename) {
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string username, password;

        size_t delimiter_pos = line.find(':');
        if (delimiter_pos != std::string::npos) {
            username = line.substr(0, delimiter_pos);
            password = line.substr(delimiter_pos + 1);
            user_credentials[username] = password; // Store in the map
        }
    }

    file.close();
}

// Function to authenticate a user
bool Authenticating_user(const std::string Username,const std::string Password){
    std::lock_guard<std::mutex> lock(clients_mutex);
    if(user_credentials.count(Username) && user_credentials[Username]==Password)return true;
    return false;
}

// Function to broadcast,direct a message to all clients except the sender
void broadcast_message(const std::string& message, int sender_socket,int flag) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    std::string new_message;
    if(flag)new_message="[broadcast from "+clients[sender_socket]+"]:"+message+'\n';
    else new_message=message+'\n';
    for (const auto& client : clients) {
        if (client.first != sender_socket) {
            if (send(client.first, new_message.c_str(), new_message.size(), 0) < 0) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "Error sending message to client " << client.second << std::endl;
            }
        }
    }
}

// Function to send a direct message to a specific user
void direct_message(const std::string& message, int client_socket,const std::string& username) {
    std::lock_guard<std::mutex> lock(clients_mutex);
            std::string new_message="["+clients[client_socket]+"]"+":"+message+'\n';
    
            if (send(sock[username], new_message.c_str(), new_message.size(), 0) < 0) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "Error sending message to client " << clients[client_socket] << std::endl;
            }
}

// Function to send a message to a specific group
void group_message(std::string gname,const std::string& message, int sender_socket,int flag,int flag2) {
    if(flag2){std::lock_guard<std::mutex> lock(clients_mutex);}
    for (const auto& client : groups[gname]) {
        if (client != sender_socket) {
            std::string new_message;
            if(flag)new_message=clients[sender_socket]+" ["+gname+"]"+":"+message+'\n';
            else new_message=message+'\n';
            if (send(client, new_message.c_str(), new_message.size(), 0) < 0) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "Error sending message to client " << clients[client] << std::endl;
            }
        } 
    }
}

// Function to create a new group
void create_group(std::string gname,int client_socket){
   std::lock_guard<std::mutex> lock(clients_mutex);
    groups[gname].insert(client_socket);
    std::string message1="Group "+gname+" is created and joined.\n";
    if (send(client_socket, message1.c_str(), message1.size(), 0) < 0) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "Error sending message to client " << clients[client_socket] << std::endl;
            }
    return;
}

// Function to join an existing group
void join_group(std::string gname,int client_socket){
    std::lock_guard<std::mutex> lock(clients_mutex);
    groups[gname].insert(client_socket);
    std::string message1="You have Joined the group:"+gname+"!\n";
    std::string message2=clients[client_socket]+" has Joined the group "+gname+"\n";
    if (send(client_socket, message1.c_str(), message1.size(), 0) < 0) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "Error sending message to client " << clients[client_socket] << std::endl;
            }
    group_message(gname,message2,client_socket,0,0);
}

// Function to leave a group
void leave_group(std::string gname,int client_socket){
    std::lock_guard<std::mutex> lock(clients_mutex);
    groups[gname].erase(client_socket);
    std::string message1="You have left the group "+gname+" !\n";
    std::string message2=clients[client_socket]+" has left the group.\n";
    if (send(client_socket, message1.c_str(), message1.size(), 0) < 0) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "Error sending message to client " << clients[client_socket] << std::endl;
            }
    group_message(gname,message2,client_socket,0,0);
}

// Function to process different types of messages
void process_message(int client_socket, std::string& message) {
    if (message.rfind("/group_msg ", 0) == 0) { // Check if the message starts with "/group_msg "
        size_t space1 = message.find(' ');
        size_t space2 = message.find(' ', space1 + 1);
        
        if (space1 != std::string::npos && space2 != std::string::npos) {
            std::string group_name = message.substr(space1 + 1, space2 - space1 - 1);
            std::string group_msg = message.substr(space2 + 1);
            if(!groups.count(group_name)){
                group_msg="Group with group name "+group_name+" not found!";
                if (send(client_socket, group_msg.c_str(), group_msg.size(), 0) < 0) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "Error sending message to client " << clients[client_socket] << std::endl;
            }
            std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "Error: Wrong group name was mentioned."<<std::endl ;
            }
            else group_message(group_name, group_msg, client_socket,1,1);
        }
    }
    else if(message.rfind("/broadcast ",0)==0){
        size_t space_pos = message.find(' ');
        
        if (space_pos != std::string::npos) {
            std::string broadcast_msg = message.substr(space_pos + 1);
            broadcast_message(broadcast_msg, client_socket,1);
        }
    }
    else if(message.rfind("/join_group ",0)==0){
        size_t space_pos = message.find(' ');
        
        if (space_pos != std::string::npos) {
            std::string gname = message.substr(space_pos + 1);
            if(!groups.count(gname)){
                std::string group_msg="Group with group name "+gname+" not found!";
                if (send(client_socket, group_msg.c_str(), group_msg.size(), 0) < 0) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "Error sending message to client " << clients[client_socket] << std::endl;
            }

            std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "Error: Wrong group name was mentioned."<<std::endl; ;
            }
            else join_group(gname, client_socket);
        }
    }
    else if(message.rfind("/msg ",0)==0){
        size_t space1 = message.find(' '); // Find first space (after "/msg")
        size_t space2 = message.find(' ', space1 + 1); // Find second space (after username)
        
        if (space1 != std::string::npos && space2 != std::string::npos) {
            std::string username = message.substr(space1 + 1, space2 - space1 - 1); // Extract username
            std::string user_message = message.substr(space2 + 1); // Extract message

            if(!sock.count(username)){
                message="User with username "+username+" not found!";
                if (send(client_socket, message.c_str(), message.size(), 0) < 0) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "Error sending message to client " << clients[client_socket] << std::endl;
            }
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "Error: Wrong username was mentioned."<<std::endl ;
            }
            
           else direct_message(user_message, client_socket,username);
        }
    }
    else if(message.rfind("/create_group ",0)==0){
        size_t space_pos = message.find(' ');
        
        if (space_pos != std::string::npos) {
            std::string gname = message.substr(space_pos + 1);
            std::string wrong_message;
            if(gname==""){
                wrong_message="Group name is Empty.";
            }
            else wrong_message="This group already exists.";

            if(gname=="" || groups.count(gname)){
                if (send(client_socket, wrong_message.c_str(), wrong_message.size(), 0) < 0) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "Error sending message to client " << clients[client_socket] << std::endl;
            }
            std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "Error: Wrong group name was mentioned."<<std::endl ;
            }
           else create_group(gname, client_socket);
        }
    }
    else if(message.rfind("/leave_group ",0)==0){

        size_t space_pos = message.find(' ');
        
        if (space_pos != std::string::npos) {
            std::string gname = message.substr(space_pos + 1);
            std::string wrong_message;
            if(gname==""){
                wrong_message="Group name is Empty.";
            }
            else if(!groups.count(gname)) wrong_message="This name group does not exists.";
            else if(!groups[gname].count(client_socket))wrong_message="You are already not a part of this group!";

            if(gname=="" || !groups.count(gname) || groups[gname].count(client_socket)){
                if (send(client_socket, wrong_message.c_str(), wrong_message.size(), 0) < 0) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "Error sending message to client " << clients[client_socket] << std::endl;
            }
            std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "Error: Wrong group name was mentioned."<<std::endl ;
            }
            else leave_group(gname, client_socket);
        }
    }
    else{
        std::string message="Please Enter message in correct format!";
        if (send(client_socket, message.c_str(), message.size(), 0) < 0) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "Error sending message to client " << clients[client_socket] << std::endl;
            }
            std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "Error:Message is given in wrong format."<<std::endl ;
    }
}

// Function to handle communication with a single client
void handle_client(int client_socket){
    char buffer[BUFFER_SIZE];
    std::string username;
    std::string password;

    // Authentication Process

    std::string prompt_username = "Enter the username: ";
    if (send(client_socket, prompt_username.c_str(), prompt_username.size(), 0) < 0) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "Failed to send username prompt to client." << std::endl;
        close(client_socket);
        return;
    }

    // Receive username
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "Failed to receive username from client." << std::endl;
        close(client_socket);
        return;
    }
    username = std::string(buffer);
    username.erase(username.find_last_not_of("\n\r") + 1); // Trim newline characters

    // 2. Ask for password
    std::string prompt_password = "Enter the password: ";
    if (send(client_socket, prompt_password.c_str(), prompt_password.size(), 0) < 0) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "Failed to send password prompt to client." << std::endl;
        close(client_socket);
        return;
    }

    // Receive password
    memset(buffer, 0, BUFFER_SIZE);
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "Failed to receive password from client." << std::endl;
        close(client_socket);
        return;
    }
    password = std::string(buffer);
    password.erase(password.find_last_not_of("\n\r") + 1); // Trim newline characters

    // Simple authentication check (accept any username/password)
    bool is_authenticated = Authenticating_user(username,password); 

    if (is_authenticated) {
        std::string welcome_msg = "Welcome to the server, " + username + "!\n";
        if (send(client_socket, welcome_msg.c_str(), welcome_msg.size(), 0) < 0) {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cerr << "Failed to send welcome message to client." << std::endl;
            close(client_socket);
            return;
        }

        // Add client to the list of connected clients
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients[client_socket]=username;
            sock[username]=client_socket;
            user_credentials[username]=password;
        }

        // Notify other clients about the new connection
        std::string join_msg = username + " has joined the chat.\n";
        broadcast_message(join_msg, client_socket,0);

        while (true) {
            memset(buffer, 0, BUFFER_SIZE);
            int recv_bytes = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if(recv_bytes <=0 ){
                //we need to remove it from all maps
                clients.erase(client_socket);
                sock.erase(username);
                user_credentials.erase(username);
                for(auto it:groups){
                    it.second.erase(client_socket);
                }
                   std::lock_guard<std::mutex> lock(cout_mutex);
                  std::cout << username << " disconnected." << std::endl;
                 close(client_socket);
                 break;
            }
            

            std::string message = std::string(buffer);
            message.erase(message.find_last_not_of("\n\r") + 1); // Trim newline characters

           process_message(client_socket,message);
            
        }
    } else {
        std::string fail_msg = "Authentication Failed\n";
        send(client_socket, fail_msg.c_str(), fail_msg.size(), 0);
        close(client_socket);
    }
}

int main() {
    int server_socket;
    sockaddr_in server_address{};

    // Create a TCP socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << "Error creating socket." << std::endl;
        return 1;
    }

    // Allow address reuse
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "setsockopt(SO_REUSEADDR) failed." << std::endl;
        close(server_socket);
        return 1;
    }

        //Define server address
        server_address.sin_family = AF_INET;
    server_address.sin_port = htons(12345);
    server_address.sin_addr.s_addr = INADDR_ANY;


    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
    std::cerr << "Error binding socket." << std::endl;
    std::cerr << "Error binding socket: " << strerror(errno) << std::endl;
    close(server_socket);
    return 1; 
 }

    // Start listening for incoming connections
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        std::cerr << "Error listening on socket." << std::endl;
        close(server_socket);
        return 1;
    }

    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Server is listening on port " << PORT << "..." << std::endl;
    }

    load_users_from_file("users.txt");

    // Vector to hold client threads
    std::vector<std::thread> client_threads;

    while (true) {
        sockaddr_in client_address{};
        socklen_t client_len = sizeof(client_address);

        // Accept a new client connection
        int client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_len);
        if (client_socket < 0) {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cerr << "Error accepting client connection." << std::endl;
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "New connection accepted." << std::endl;
        }

        // Create a thread to handle the new client
        client_threads.emplace_back(std::thread(handle_client, client_socket));
        // , client_address));

        // Optional: Limit the number of clients (e.g., 2)
        if (client_threads.size() >= MAX_CLIENTS) {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "Maximum client limit reached. No longer accepting new connections." << std::endl;
            break;
        }
    }

    // Join all client threads before exiting
    for (auto& th : client_threads) {
        if (th.joinable()) {
            th.join();
        }
    }

    // Close the server socket
    close(server_socket);
    return 0;
}

