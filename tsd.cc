/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>
#include <sys/wait.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "sns.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using csce662::Message;
using csce662::ListReply;
using csce662::Request;
using csce662::Reply;
using csce662::SNSService;
using csce662::Regmessage;

struct Client {
  std::string username;
  bool connected = true;
  int following_file_size = 0;
  std::vector<int> client_followers;
  std::vector<int> client_following;
  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
};

enum serverRole{
  router,
  router_slave,
  master,
  slave
} role;

struct ServerInfo {
  std::string hostname;
  std::string port;
  bool isAvailable;
};

//global database
std::vector<ServerInfo> server_db;

std::vector<Client> client_db;

//Helper function used to find a Client object given its username
int find_user(std::string username){
  for(int i=0; i<client_db.size(); ++i){
    if(client_db[i].username == username)
      return i;
  }
  return -1;
}

int find_server(std::string hostname, std::string port){
  for(int i=0; i<server_db.size(); ++i){
    if(server_db[i].hostname == hostname && server_db[i].port == port)
      return i;
  }
  return -1;
}

class SNSServiceImpl final : public SNSService::Service {
  Status Register(ServerContext* context, const Regmessage* request, Reply* reply) override {
    int index = find_server(request->hostname(),request->port());
    if(index>=0){
      server_db[index].isAvailable = true;
      std::cout<<"The crash server "<<request->hostname()<<":"<<request->port()<<" is availabe again"<<std::endl;
    }
    else{
      ServerInfo server = {request->hostname(), request->port(), true};
      server_db.push_back(server);
      std::cout<<"A new server "<<request->hostname()<<":"<<request->port()<<" is added"<<std::endl;
    }

    return Status::OK;
  }
  //report a crash server
  Status ReportCrash(ServerContext* context, const Regmessage* request, Reply* reply) override {
    int index = find_server(request->hostname(),request->port());
    if(index>=0){
      server_db[index].isAvailable = false;
      std::cout<<"Server "<<request->hostname()<<":"<<request->port()<<" is unavailabe now"<<std::endl;
    }
    return Status::OK;
  }

  //master
  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
    Client user = client_db[find_user(request->username())];
    int index = 0;
    for(Client c : client_db){
      list_reply->add_all_users(c.username);
    }
    std::vector<int>::const_iterator it;
    for(it = user.client_followers.begin(); it!=user.client_followers.end(); it++){
      list_reply->add_followers(client_db[*it].username);
    }
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    std::string username1 = request->username();
    std::string username2 = request->arguments(0);
    int index1 = find_user(username1);
    int index2 = find_user(username2);
    if(index2 < 0 || username1 == username2)
      reply->set_msg("Join Failed -- Invalid Username");
    else{
      
      // Client *user1 = &(client_db[find_user(username1)]);
      // Client *user2 = &(client_db[join_index]);

      //std::cout<<"Now "<<client_db[index2].username<<"'s all followers are: "<<std::endl;
      for (auto i : client_db[index2].client_followers) {
          std::cout << client_db[i].username << std::endl;
      }
      // std::cout<<"Now "<<user1->username<<"'s all followings are: "<<std::endl;
      // for (auto user : user1->client_following) {
      //     std::cout << user->username << std::endl;
      // }

      if(std::find(client_db[index1].client_following.begin(), client_db[index1].client_following.end(), index2) != client_db[index1].client_following.end()){
	      reply->set_msg("Join Failed -- Already Following User");
        return Status::OK;
      }
      client_db[index1].client_following.push_back(index2);
      client_db[index2].client_followers.push_back(index1);
      reply->set_msg("Join Successful");
      // std::cout<<"After "<<client_db[index1].username<<" follows "<<client_db[index2].username<<", "<<std::endl;
      // std::cout<<client_db[index2].username<<"'s all followers are: "<<std::endl;
      // for (auto i : client_db[index2].client_followers) {
      //     std::cout << client_db[i].username << std::endl;
      //     std::cout << client_db[i].connected << std::endl;
      // }
      // std::cout<<user1->username<<"'s all followings are: "<<std::endl;
      // for (auto user : user1->client_following) {
      //     std::cout << user->username << std::endl;
      // }

    }
    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    std::string username1 = request->username();
    std::string username2 = request->arguments(0);
    int index1 = find_user(username1);
    int index2 = find_user(username2);
    if(index2 < 0 || username1 == username2)
      reply->set_msg("Leave Failed -- Invalid Username");
    else{
      // Client *user1 = &client_db[find_user(username1)];
      // Client *user2 = &client_db[leave_index];
      if(std::find(client_db[index1].client_following.begin(), client_db[index1].client_following.end(), index2) == client_db[index1].client_following.end()){
	      reply->set_msg("Leave Failed -- Not Following User");
        return Status::OK;
      }
      client_db[index1].client_following.erase(find(client_db[index1].client_following.begin(), client_db[index1].client_following.end(), index2)); 
      client_db[index2].client_followers.erase(find(client_db[index2].client_followers.begin(), client_db[index2].client_followers.end(), index1));
      reply->set_msg("Leave Successful");
    }
    return Status::OK;
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    
    if(role==router){ // routing server find a master available server
        //std::cout<<"DB size is "<<server_db.size()<<std::endl;
        std::cout<<"routing server is running"<<std::endl;
        for(int i = 0; i< server_db.size(); i++){
          if(server_db[i].isAvailable){
            std::cout<<server_db[i].hostname + " " + server_db[i].port<<" is available right now"<<std::endl;
            reply->set_msg("Login Successful!");
            reply->set_hostname(server_db[i].hostname);
            reply->set_port(server_db[i].port);
            //std::cout<<server_db[i].hostname<<":"<<server_db[i].port<<" was routed"<<std::endl;
            break;
          }
        }
    }
    else if (role==master){
      std::cout<<"master server is running"<<std::endl;
      Client c;
      std::string username = request->username();
      std::cout<<username + " is trying to connect me"<<std::endl;
      int user_index = find_user(username);
      if(user_index < 0){
        c.username = username;
        client_db.push_back(c);
        reply->set_msg("Login Successful!");
        //std::cout<<"Now we have "<< client_db.size() <<" users:"<<std::endl;
        for (auto user : client_db) {
          std::cout << user.username << std::endl;
        }
      }
      else{ 
        Client *user = &client_db[user_index];
        if(user->connected)
          reply->set_msg("Invalid Username");//already existed
        else{
          std::string msg = user->username + " comes back";
	        reply->set_msg(msg);
          user->connected = true;
        }
      }
    }
    return Status::OK;
  }

  Status Timeline(ServerContext* context, 
		ServerReaderWriter<Message, Message>* stream) override {
    Message message;
    //Client *c;
    int user_index ;
    while(stream->Read(&message)) {
      std::string username = message.username();
      user_index = find_user(username);
      //c = &client_db[user_index];
 
      //Write the current message to "username.txt"
      std::string filename = username+".txt";
      std::ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
      google::protobuf::Timestamp temptime = message.timestamp();
      std::string time = google::protobuf::util::TimeUtil::ToString(temptime);
      std::string fileinput = time+" :: "+message.username()+":"+message.msg()+"\n";
      //"Set Stream" is the default message from the client to initialize the stream
      if(message.msg() != "Set Stream")
        user_file << fileinput;
      //If message = "Set Stream", print the first 20 chats from the people you follow
      else{
        if(client_db[user_index].stream==0)
      	  client_db[user_index].stream = stream;
        std::string line;
        std::vector<std::string> newest_twenty;
        std::ifstream in(username+"following.txt");
        int count = 0;
        //Read the last up-to-20 lines (newest 20 messages) from userfollowing.txt
        while(getline(in, line)){
          if(client_db[user_index].following_file_size > 20){
	          if(count < client_db[user_index].following_file_size-20){
              count++;
	            continue;
            }
          }
          newest_twenty.push_back(line);
        }
        Message new_msg; 
        //Send the newest messages to the client to be displayed
        for(int i = 0; i<newest_twenty.size(); i++){
          new_msg.set_msg(newest_twenty[i]);
          stream->Write(new_msg);
        }   
        continue;
      }
      //Send the message to each follower's stream
      std::vector<int>::const_iterator it;
      for(it = client_db[user_index].client_followers.begin(); it!=client_db[user_index].client_followers.end(); it++){
        int index = *it;
      	if(client_db[index].stream!=0 && client_db[index].connected)
	        client_db[index].stream->Write(message);
        //For each of the current user's followers, put the message in their following.txt file
        std::string temp_username = client_db[index].username;
        std::string temp_file = temp_username + "following.txt";
        std::ofstream following_file(temp_file,std::ios::app|std::ios::out|std::ios::in);
        following_file << fileinput;
        client_db[user_index].following_file_size++;
	      std::ofstream user_file(temp_username + ".txt",std::ios::app|std::ios::out|std::ios::in);
        user_file << fileinput;
      }
    }
    //If the client disconnected from Chat Mode, set connected to false
    client_db[user_index].connected = false;
    return Status::OK;
  }

};

void RunRoutingServer(std::string hostname, std::string port_no) {
  std::string server_address = hostname+":"+port_no;
  SNSServiceImpl service;
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Routing Server is listening on " << server_address << std::endl;

  server->Wait();

}

void RunMasterServer(std::string hostname, std::string port_no) {
  std::string login_info = "localhost:3010";//the default routing server
  std::unique_ptr<SNSService::Stub> stubR_; //build the stub of the routing server
  stubR_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
            grpc::CreateChannel(
              login_info, grpc::InsecureChannelCredentials())));

  Regmessage request;
  request.set_hostname(hostname);
  request.set_port(port_no);
  Reply reply;
  ClientContext context;
  //Let the routing server register the master server
  
  //Just like RunRoutingServer does
  std::string server_address = hostname+":"+port_no;
  SNSServiceImpl service;
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Master Server is listening on " << server_address << std::endl;
  Status status = stubR_->Register(&context, request, &reply);
  server->Wait();
}

//Here we need to run twp processes which ere master and slave respectively
void RunMasterSlaveServers(std::string hostname, std::string port_no){
  std::string mode;
  if(role == slave)
    mode = "M";
  else //role == router_slave
    mode = "R";
  
  pid_t c_pid;
  //int s;
  bool flag = false;
  while (true){
    //std::cout<<"process run again"<<std::endl;  
    if (flag) {
        sleep(25);
    }
    flag = false;
    c_pid = fork();
    if (c_pid < 0) {
      perror("Fail to create master/slave servers");
      exit(EXIT_FAILURE);
    } 
    else if(c_pid == 0){  //child process, which runs master server
      //std::cout<<"master process is running 1: "<<flag<<std::endl;
      //std::cout<<"master process is running 2: "<<flag<<std::endl;
      //char *const args[8] = {"tsd", "-p", (char*)port_no.c_str(), "-h", (char*)hostname.c_str(), "-r", (char*)mode.c_str(), NULL};
      //execv("./tsd", args);
      if(mode=="M") {
        role=master;
        RunMasterServer(hostname, port_no);
      }
      else if(mode=="R") {
        role=router;
        RunRoutingServer(hostname, port_no);
      }

    }
    
    else {
      std::cout<<"slave process is monitoring master process "<<c_pid<<std::endl;
      std::string login_info = "localhost:3010";//the default routing server
      std::unique_ptr<SNSService::Stub> stubR_; //build the stub of the routing server
      stubR_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
                grpc::CreateChannel(
                  login_info, grpc::InsecureChannelCredentials())));
      //std::cout<<"slave process is running 1: "<<flag<<std::endl;
      //while (true){
        int st;
        //WNOHANG
        int s = waitpid(c_pid, &st, 0); // non-blocking wait function
        if(s != 0){
          //Report to routing if not available
          //std::cout<<"master process "<<s<<" breaks"<<std::endl;
          flag = true;
          //std::cout<<"slave process is running 2: "<<flag<<std::endl;
          Regmessage request;
          request.set_hostname(hostname);
          request.set_port(port_no);
          Reply reply;
          ClientContext context;
          //Let the routing server register the master server
          std::cout<<"We need to report a crash"<<std::endl;
          Status status = stubR_->ReportCrash(&context, request, &reply);
          //break;
          //Here we exit from this while loop and reneter the outer circulation that creats a new child process 
          //and reboots the master server
        }
      //}
    }
    
  }
}

int main(int argc, char** argv) {
  
  std::string port = "3010";
  std::string hostname = "localhost";
  std::string mode="";
  int opt = 0;
    
  while ((opt = getopt(argc, argv, "p:h:r:")) != -1){
     
    switch(opt) {
      case 'p':
          port = optarg;break;
      case 'h':
          hostname = optarg;break;
      case 'r':
          mode = optarg;break;
      default:
	  std::cerr << "Invalid Command Line Argument\n";
    }
  }
   
  if(mode=="R"){
    role=router;
    RunRoutingServer(hostname,port);
  }
  if(mode=="M"){
    role=master;
    RunMasterServer(hostname,port);
  }
  if(mode=="RS"){
    role=router_slave;
    RunMasterSlaveServers(hostname,port);
  }
  if(mode=="MS"){
    role=slave;
    RunMasterSlaveServers(hostname,port);
  }

  return 0;
}
