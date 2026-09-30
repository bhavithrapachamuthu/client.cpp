#include <stdio.h>
#include <WinSock2.h>
#include <cstring>
#include<thread>
#include<mutex>
#include<time.h>
using namespace std;
#pragma comment(lib,"ws2_32.lib")
#define PORT 8080
#define PACKET_EMPLOYEE_ID 1
#define PACKET_EMPLOYEE_NAME 2
#define PACKET_EMPLOYEE_SALARY 3
#define PACKET_CUSTOMER_ID 4
#define PACKET_CUSTOMER_NAME 5
#define PACKET_CUSTOMER_ADDRESS 6
#define PACKET_SALE_ID 7
#define PACKET_SALE_AMOUNT 8
#define PACKET_SALE_DATE 9
#define PACKET_SALE_EMP_ID 10
#define PACKET_SALE_CUS_ID 11
#define PACKET_RESULT 12
#define PACKET_VIEW_ALL 13
#define PACKET_DELETE 14
#define PACKET_SEARCH 15
#define PACKET_ADD_EMPLOYEE 16
#define PACKET_ADD_CUSTOMER 17
#define PACKET_ADD_SALE 18
#define FRAME_SINGLE 0
#define FRAME_MORE 1
struct Packet{
    unsigned char header;
    unsigned char length;
    unsigned char data[255];
};
class Data{
    public:
    int id;
};
class Employee : public Data{
    public:
    char empName[50];
    float salary;
};
class Customer : public Data{
    public:
    char cusName[50];
    char address[100];
};
class Sale : public Data{
    public:
    int empId;
    int cusId;
    float amount;
    char date[26];
};
template<class T>
struct Node{
    T data;
    Node<T>* left;
    Node<T>* right;
    Node()
    {
        left = NULL;
        right = NULL;
    }
};
Node<Employee>* empRoot = NULL;
Node<Customer>* cusRoot = NULL;
Node<Sale>* saleRoot = NULL;
mutex file;
mutex values;
template<typename T>
void insert(Node<T>*& root,const T&data){
    if(root==NULL){
        root=new Node<T>;
        root->data=data;
        return;
    }
    if(data.id<root->data.id){
        insert(root->left,data);
    }
    else if(data.id>root->data.id){
        insert(root->right,data);
    }
}
template<typename T>
Node<T>* find(Node<T>* root, int id){
    while(root != NULL)
    {
        if(id == root->data.id)
            return root;

        if(id < root->data.id)
            root = root->left;
        else
            root = root->right;
    }
    return NULL;
}
Node<Employee>*findbyname(Node<Employee>*root,const char*name){
    if(root==NULL){
        return NULL;
    }
    Node<Employee>*result=findbyname(root->left,name);
    if(result!=NULL){
        return result;
    }
    if(strcmp(root->data.empName,name)==0){
        printf("MATCH FOUND\n");
        return root;
    }
    return findbyname(root->right,name);
}

template<typename T>
Node<T>*deleteNode(Node<T>*root,int id){
    if(root==NULL){
        return NULL;
    }
    if(id< root->data.id){
        root->left=deleteNode(root->left,id);
    }
    else if(id>root->data.id){
        root->right=deleteNode(root->right,id);
    }
    else{
        if(root->left==NULL && root->right==NULL){
            delete root;
            return NULL;
        }
        else if(root->left==NULL){
            Node<T>*temp=root->right;
            delete root;
            return temp;
        }
        else if(root->right==NULL){
            Node<T>*temp=root->left;
            delete root;
            return temp;
        }
        else{
            Node<T>*temp=root->right;
            while(temp->left!=NULL){
               temp=temp->left; 
            }
            root->data=temp->data;
            root->right=deleteNode(root->right,temp->data.id);    
        }
    }
    return root;
}
unsigned char makeHeader(int packetID,int frameType){
    return ((packetID & 0x0F) << 1) | (frameType & 0x01);
}
int getPacketID(unsigned char header){
    return (header >> 1) & 0x0F;
}
int getFrameType(unsigned char header){
    return header & 0x01;
}
bool sendAll(SOCKET sock,const char*buffer,int length){
    int total=0;
    while(total < length){
        int n=send(sock,buffer+total,length-total,0);
        if(n <= 0)
        return false;
        total+=n;
    } 
    return true;
}
bool recvAll(SOCKET sock,char* buffer,int length){
    int total = 0;
    while(total < length)
    {
        int n=recv(sock,buffer+total,length-total,0);
        if(n <= 0)
            return false;
        total+=n;
    }
    return true;
}
bool sendPacket(SOCKET client,int packetID,int length,int frameType,const void* data){
    if(length<0 || length>254){
        return false;
    }
    unsigned char header =makeHeader(packetID,frameType);
    unsigned char len=(unsigned char)length;
    if(!sendAll(client,(char*)&header,1)){
        return false;
    }
    if(!sendAll(client,(char*)&len,1)){
        return false;
    }
    if(length>0){
        if(!sendAll(client,(const char*)data,length)){
            return false;
        }
    }
    return true;
}
bool receivePacket(SOCKET client,Packet& packet){
    memset(&packet,0,sizeof(packet));
    if(!recvAll(client,(char*)&packet.header,1)){
        return false;
    }
    if(!recvAll(client,(char*)&packet.length,1)){
        return false;
    }
    if(packet.length>254)
    return false;
    if(packet.length > 0){
        if(!recvAll(client,(char*)packet.data,packet.length)){
            return false;
        }
        packet.data[packet.length]='\0';
    }
    int packetId=getPacketID(packet.header);
    int frametype=getFrameType(packet.header);
    printf("\n----------------------------------\n");
    printf("Packet Id: %d\n",packetId);
    printf("Length: %d\n",packet.length);
    if(frametype==FRAME_MORE){
        printf("Frame Type: MORE\n");
    }
    else{
        printf("Frame Type: SINGLE\n");
    }
    printf("\n----------------------------------\n");
    return true;
}
void sendResult(SOCKET client,const char* msg){
    int length = (int)strlen(msg);
    if(length > 254)
    length = 254;
    sendPacket(client,PACKET_RESULT,length,FRAME_SINGLE,msg);
}
void saveallEmp(Node<Employee>*root,FILE*f){
    if(root==NULL){
        return;
    }
    saveallEmp(root->left,f);
    unsigned char type=1;
    fwrite(&type,sizeof(type),1,f);
    fwrite(&root->data,sizeof(Employee),1,f);
    saveallEmp(root->right,f);
}
void saveallCus(Node<Customer>*root,FILE*f){
    if(root==NULL){
        return;
    }
    saveallCus(root->left,f);
    unsigned char type=2;
    fwrite(&type,sizeof(type),1,f);
    fwrite(&root->data,sizeof(Customer),1,f);
    saveallCus(root->right,f);
}
void saveallSale(Node<Sale>*root,FILE*f){
    if(root==NULL){
        return;
    }
    saveallSale(root->left,f);
    unsigned char type=3;
    fwrite(&type,sizeof(type),1,f);
    fwrite(&root->data,sizeof(Sale),1,f);
    saveallSale(root->right,f);
}
void savefile(){
    lock_guard<mutex>lock(file);
    FILE*f=fopen("data.bin","wb");
    if(f==NULL){
        printf("File saving Failed\n");
        return;
    }
    saveallEmp(empRoot,f);
    saveallCus(cusRoot,f);
    saveallSale(saleRoot,f);
    fclose(f);
    printf("File updated sucessfully\n");
}
template<typename T>
void loadvalue(FILE*f,Node<T>*&root){
    T data;
    if(fread(&data,sizeof(T),1,f)==1){
        if(find(root,data.id)== NULL){
            insert(root,data);
        }
    }
}
void loadfile(){
    FILE*f=fopen("data.bin","rb");
    if(f==NULL){
        printf("No old data found!\n");
        return;
    }
    unsigned char type;
    while (fread(&type,sizeof(type),1,f)==1){
        if(type==1){
            loadvalue(f,empRoot);
        }
        else if(type==2){
            loadvalue(f,cusRoot);
        }
        else if(type==3){
            loadvalue(f,saleRoot);
        }
        else{
            printf("Invalid!");
            break;
        }
    }
    fclose(f);
    printf("\nData loaded successfully\n");
}
void handleAddEmp(SOCKET client,Packet&p){
    if(p.length!=sizeof(Employee)){
        sendResult(client,"INVALID EMPLOYEE DATA");
        return;
    }
    Employee e;
    memcpy(&e,p.data,sizeof(Employee));
    lock_guard<mutex>lock(values);
    if(find(empRoot,e.id)!=NULL){
        sendResult(client,"EMPLOYEE ALREADY EXISTS");
        return;
    }
    insert(empRoot,e);
    savefile();
    sendResult(client,"EMPLOYEE ADDED");
}
void handleAddCus(SOCKET client,Packet&p){
    if(p.length!=sizeof(Customer)){
        sendResult(client,"INVALID CUSTOMER DATA");
        return;
    }
    Customer c;
    memcpy(&c,p.data,sizeof(Customer));
    lock_guard<mutex>lock(values);
    if(find(cusRoot,c.id)!=NULL){
        sendResult(client,"CUSTOMER ALREADY EXISTS");
        return;
    }
    insert(cusRoot,c);
    savefile();
    sendResult(client,"CUSTOMER ADDED");
}
void handleAddSale(SOCKET client,Packet&p){
    if(p.length!=sizeof(Sale)){
        sendResult(client,"INVALID SALE DATA");
        return;
    }
    Sale s;
    memcpy(&s,p.data,sizeof(Sale));
    lock_guard<mutex>lock(values);
    if(find(saleRoot,s.id)!=NULL){
        sendResult(client,"SALE ALREADY EXISTS");
        return;
    }
    insert(saleRoot,s);
    savefile();
    sendResult(client,"SALE ADDED");
}
void sendEmployee(Node<Employee>*root,SOCKET client){
    if(root==NULL){
        return;
    }
    sendEmployee(root->left,client);
    sendPacket(client,PACKET_EMPLOYEE_ID,sizeof(int),FRAME_MORE,&root->data.id);
    sendPacket(client,PACKET_EMPLOYEE_NAME,strlen(root->data.empName)+1,FRAME_MORE,root->data.empName);
    sendPacket(client,PACKET_EMPLOYEE_SALARY,sizeof(float),FRAME_MORE,&root->data.salary);
    sendEmployee(root->right,client);
}
void sendCustomer(Node<Customer>*root,SOCKET client){
    if(root==NULL){
        return;
    }
    sendCustomer(root->left,client);
    sendPacket(client,PACKET_CUSTOMER_ID,sizeof(int),FRAME_MORE,&root->data.id);
    sendPacket(client,PACKET_CUSTOMER_NAME,strlen(root->data.cusName)+1,FRAME_MORE,root->data.cusName);
    sendPacket(client,PACKET_CUSTOMER_ADDRESS,strlen(root->data.address)+1,FRAME_MORE,root->data.address);
    sendCustomer(root->right,client);
}
void sendSale(Node<Sale>*root,SOCKET client){
    if(root==NULL){
        return;
    }
    sendSale(root->left,client);
    sendPacket(client,PACKET_SALE_ID,sizeof(int),FRAME_MORE,&root->data.id);
    sendPacket(client,PACKET_SALE_EMP_ID,sizeof(int),FRAME_MORE,&root->data.empId);
    sendPacket(client,PACKET_SALE_CUS_ID,sizeof(int),FRAME_MORE,&root->data.cusId);
    sendPacket(client,PACKET_SALE_AMOUNT,sizeof(float),FRAME_MORE,&root->data.amount);
    sendPacket(client,PACKET_SALE_DATE,strlen(root->data.date)+1,FRAME_MORE,root->data.date);
    sendSale(root->right,client);
}
void searchSale(Node<Sale>*root,SOCKET client,int empId){
    if(root==NULL){
        return;
    }
    searchSale(root->left,client,empId);
    if(root->data.empId==empId){
        sendPacket(client,PACKET_SALE_ID,sizeof(int),FRAME_MORE,&root->data.id);
        sendPacket(client,PACKET_SALE_EMP_ID,sizeof(int),FRAME_MORE,&root->data.empId);
        sendPacket(client,PACKET_SALE_CUS_ID,sizeof(int),FRAME_MORE,&root->data.cusId);
        sendPacket(client,PACKET_SALE_AMOUNT,sizeof(float),FRAME_MORE,&root->data.amount);
        sendPacket(client,PACKET_SALE_DATE,strlen(root->data.date)+1,FRAME_MORE,root->data.date);
    }
    searchSale(root->right,client,empId);
}
void handleSearch(SOCKET client,Packet &p){
    if(p.length ==0 || p.length>=50){
        sendResult(client,"INVALID NAME");
        return;
    }
    char name[50]={0};
    memcpy(name,p.data,p.length);
    name[p.length]='\0';
    lock_guard<mutex>lock(values);
    Node<Employee>*e=findbyname(empRoot,name);
    if(e==NULL){
        sendResult(client,"EMPLOYEE NOT FOUND");
        return;
    }
    sendPacket(client,PACKET_EMPLOYEE_ID,sizeof(int),FRAME_MORE,&e->data.id);
    sendPacket(client,PACKET_EMPLOYEE_NAME,strlen(e->data.empName),FRAME_MORE,e->data.empName);
    sendPacket(client,PACKET_EMPLOYEE_SALARY,sizeof(float),FRAME_MORE,&e->data.salary);
    searchSale(saleRoot,client,e->data.id);
    sendResult(client,"END");
}
void deleterelatedSales(Node<Sale>*root,int id,int type){
    if(root==NULL)
    return;
    deleterelatedSales(root->left,id,type);
    if(type==1 && root->data.empId==id){
        saleRoot=deleteNode(saleRoot,root->data.id);
    }
    else if(type==2 && root->data.cusId==id){
        saleRoot=deleteNode(saleRoot,root->data.id);
    }
    deleterelatedSales(root->right,id,type);
}
void handleDeleteEmp(SOCKET client,Packet&p){
    if(p.length!=5){
        sendResult(client,"INVALID EMPLOYEE ID");
        return;
    }
    int id;
    memcpy(&id,p.data+1,sizeof(int));
    if(find(empRoot,id)==NULL){
        sendResult(client,"EMPLOYEE NOT FOUND");
        return;
    }
    deleterelatedSales(saleRoot,id,1);
    empRoot=deleteNode(empRoot,id);
    savefile();
    printf("\nEmployee deleted\n");
    sendResult(client,"EMPLOYEE DELETED");
}
void handleDeleteCus(SOCKET client,Packet&p){
    if(p.length!=5){
        sendResult(client,"INVALID CUSTOMER ID");
        return;
    }
    int id;
    memcpy(&id,p.data+1,sizeof(int));
    if(find(cusRoot,id)==NULL){
        sendResult(client,"CUSTOMER NOT FOUND");
        return;
    }
    deleterelatedSales(saleRoot,id,2);
    cusRoot=deleteNode(cusRoot,id);
    savefile();
    printf("\nCustomer deleted\n");
    sendResult(client,"CUSTOMER DELETED");
}
void handleDeleteSale(SOCKET client,Packet&p){
    if(p.length!=5){
        sendResult(client,"INVALID SALE ID");
        return;
    }
    int id;
    memcpy(&id,p.data+1,sizeof(int));
    if(find(saleRoot,id)==NULL){
        sendResult(client,"SALE NOT FOUND");
        return;
    }
    saleRoot=deleteNode(saleRoot,id);
    savefile();
    printf("\nSale deleted\n");
    sendResult(client,"SALE DELETED");
}
DWORD WINAPI clienthandler(LPVOID lpParam){
    SOCKET client=(SOCKET)lpParam;
    printf("\nNEW CLIENT THREAD STARTED\n");
    printf("Client socket=%llu\n",(unsigned long long)client);
    while(true){
        Packet p;
        if(!receivePacket(client,p)){
            printf("Client disconnected\n");
            break;
        }
        int packetId=getPacketID(p.header);
        switch(packetId){
            case PACKET_ADD_EMPLOYEE:
            handleAddEmp(client,p);
            break;

            case PACKET_ADD_CUSTOMER:
            handleAddCus(client,p);
            break;

            case PACKET_ADD_SALE:
            handleAddSale(client,p);
            break;

            case PACKET_VIEW_ALL:
            {
                if(p.length!=1){
                    sendResult(client,"INVALID VIEW DATA");
                    break;
                }
                int type=p.data[0];
                lock_guard<mutex>lock(values);
                if(type==1){
                    sendEmployee(empRoot,client);
                    sendResult(client,"END");
                }
                else if(type==2){
                    sendCustomer(cusRoot,client);
                    sendResult(client,"END");
                }
                else if(type==3){
                    sendSale(saleRoot,client);
                    sendResult(client,"END");
                }
                else{
                    sendResult(client,"INVALID VIEW TYPE");
                }
            }
            break;

            case PACKET_SEARCH:
            handleSearch(client,p);
            break;

            case PACKET_DELETE:
            {
                if(p.length!=5){
                sendResult(client,"INVALID DELETE DATA");
                break;
                }
                int id;
                memcpy(&id,p.data+1,sizeof(int));
                if(p.data[0]==1){
                    handleDeleteEmp(client,p);
                }
                else if(p.data[0]==2){
                    handleDeleteCus(client,p);
                }
                else if(p.data[0]==3){
                    handleDeleteSale(client,p);
                }
                else{
                    sendResult(client,"INVALID DELETE TYPE");
                }
                break;
            }
            default:
            printf("Unknown Packet Id: %d\n",packetId);
            break;
        }
    }
    closesocket(client);
    return 0;
}
int main(){
    WSADATA wsa;
    if(WSAStartup(MAKEWORD(2,2),&wsa)!=0){
        printf("WSAStartup failed!\n");
        return 1;
    }
    SOCKET serverSocket =socket(AF_INET,SOCK_STREAM,0);
    if(serverSocket==INVALID_SOCKET){
        printf("Socket creation failed\n");
        WSACleanup();
        return 1;
    }
    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr =inet_addr("127.0.0.1");
    if(bind(serverSocket,(sockaddr*)&server,sizeof(server))==SOCKET_ERROR){
        printf("Bined failed\n");
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    if(listen(serverSocket, 5)==SOCKET_ERROR){
        printf("Listen failed\n");
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    printf("Server Started\n");
    loadfile();
    while(true){
        SOCKET client =accept(serverSocket,NULL,NULL);
        if(client==INVALID_SOCKET){
            continue;
        }
        printf("\nClient Connected\n");
        DWORD threaId;
        HANDLE thread=CreateThread(NULL,0,clienthandler,(LPVOID)client,0,&threaId);
        if(thread==NULL){
            printf("Thread creation failed\n");
            closesocket(client);
            continue;
        }
        CloseHandle(thread);
    }
    savefile();
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}