#include <stdio.h>
#include <WinSock2.h>
#include <Windows.h>
#include <cstring>
#include<string>
#include<fstream>
#include<thread>
#include<mutex>
#include<time.h>
#include<vector>
#include<nlohmann/json.hpp>
using namespace std;
using json=nlohmann::json;
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
class Value{
    public:
    int id;
};
class Employee : public Value{
    public:
    char empName[50];
    float salary;
};
class Customer : public Value{
    public:
    char cusName[50];
    char address[100];
};
class Sale : public Value{
    public:
    int empId;
    int cusId;
    float amount;
    char date[26];
};
struct clientdata{
    SOCKET client;
    int clientid;
    vector<Employee> employees;
    vector<Customer> customers;
    vector<Sale> sales;
};
vector<clientdata> allclientdata;
clientdata* getclientdata(int clientid)
{
    for(size_t i = 0;i < allclientdata.size();i++)
    {
        if(allclientdata[i].clientid == clientid)
        {
            return &allclientdata[i];
        }
    }
    return NULL;
}
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
template<typename T>
int countNode(Node<T>*root){
    if(root==NULL){
        return 0;
    }
    else{
        return 1+countNode(root->left)+countNode(root->right);
    }
}
unsigned char makeHeader(int packetID,int frameType){
    return (unsigned char)packetID;
}
int getPacketID(unsigned char header){
    return (unsigned char)header;
}
int getFrameType(unsigned char header){
    return FRAME_SINGLE;
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
bool receiveResult(SOCKET client){
    Packet packet;
    if(!receivePacket(client,packet)){
        return false;
    }
    int packetID =getPacketID(packet.header);
    if(packetID!=PACKET_RESULT){
        return false;
    }
    char result[256];
    int len=packet.length;
    if(len>255)
    len=255;
    memcpy(result,packet.data,len);
    result[packet.length]='\0';
    printf("\n %s\n",result);
    return true;
}
void sendEmployees(SOCKET client, clientdata *group)
{
    for(size_t i = 0; i < group->employees.size(); i++)
    {
        Employee e;
        memset(&e, 0, sizeof(e));
        e.id = group->employees[i].id;
        strcpy_s(e.empName, sizeof(e.empName),group->employees[i].empName);
        e.salary = group->employees[i].salary;
        insert(empRoot, e);
        sendPacket(client, PACKET_ADD_EMPLOYEE,sizeof(Employee),FRAME_SINGLE,&e);
        receiveResult(client);
    }
}
/* void generateCus(SOCKET client,int cusCount,int cusId,const char*cusName,const char*cusAddress){
    for(int i=0;i<cusCount;i++){
        Customer c;
        memset(&c,0,sizeof(c));
        c.id=cusId+1;
        sprintf_s(c.cusName,sizeof(c.cusName),"%s %d",cusName,c.id);
        sprintf_s(c.address,sizeof(c.address),"%s %d",cusAddress,c.id);
        insert(cusRoot,c);
        sendPacket(client,PACKET_ADD_CUSTOMER,sizeof(Customer),FRAME_SINGLE,&c);
        receiveResult(client);
    }
}
void generateSales(SOCKET client,int saleCount,int saleId,float saleAmount,float amountIncrement){
    int empTotal=countNode(empRoot);
    int cusTotal=countNode(cusRoot);
    if(empTotal==0 || cusTotal==0)
    return;
    for(int i=0;i<saleCount;i++){
        Sale s;
        memset(&s,0,sizeof(s));
        s.id=saleId+1;
        s.empId=(i % empTotal)+1;
        s.cusId=(i % cusTotal)+1;
        s.amount=saleAmount+i*(amountIncrement);
        time_t now=time(NULL);
        strcpy_s(s.date,sizeof(s.date),ctime(&now));
        insert(saleRoot,s);
        sendPacket(client,PACKET_ADD_SALE,sizeof(Sale),FRAME_SINGLE,&s);
        receiveResult(client);
    }
} */
void loadJson(){
    ifstream file("data.json");
    if(!file){
        printf("File open failed\n");
        return;
    }
    json j;
    file>>j;
}
void jsonConfig(SOCKET client,int clientid){
    clientdata* group=getclientdata(clientid);
    if(group==NULL){
        printf("Client %d no Config found\n",clientid);
        return;
    }
    printf("[client %d]employees:%d\n",clientid,(int)group->employees.size());
    printf("[client %d]customers:%d\n",clientid,(int)group->customers.size());
    printf("[client %d]sales:%d\n",clientid,(int)group->sales.size());
    sendEmployees(client,group);
}
void displayEmp(SOCKET client){
    printf("\n========EMPLOYEE DETAILS========\n");
    printf("%-10s %-20s %-15s\n","ID","EMPLOYEE NAME","SALARY");
    printf("----------------------------------------------------\n");
    Packet p;
    int id=0;
    char name[50];
    float salary=0;
    while(true){
        if(!receivePacket(client,p)){
            printf("\nServer disconnected\n");
            return;
        }
        int packetID =getPacketID(p.header);
        if(packetID==PACKET_RESULT){
            if(p.length == 3 && memcmp(p.data,"END",3) == 0){
                printf("Emp display completed\n");
                return;
            }
            p.data[p.length]='\0';
            printf("%s\n",p.data);
        }
        else if(packetID == PACKET_EMPLOYEE_ID){
            int id;
            memcpy(&id,p.data,sizeof(int));
            printf("%-10d ",id);
        }
        else if(packetID==PACKET_EMPLOYEE_NAME){
            char name[50];
            int len=p.length;
            if(len>=sizeof(name))
            len=sizeof(name)-1;
            memcpy(name,p.data,len);
            name[len]='\0';
            printf("%-20s ",name);
        }
        else if(packetID==PACKET_EMPLOYEE_SALARY){
            float salary;
            memcpy(&salary,p.data,sizeof(float));
            printf("%-15.2f\n",salary);
        }
        else{
            printf("Unknown Packet Id: %d\n",packetID);
        }
    }
    printf("----------------------------------------------------\n");
}
void displayCus(SOCKET client){
    printf("\n========CUSTOMER DETAILS========\n");
    printf("%-10s %-20s %-20s\n","ID","CUSTOMER NAME","ADDRESS");
    printf("----------------------------------------------------\n");
    Packet p;
    int id=0;
    char name[50];
    char address[100];
    while(true){
        if(!receivePacket(client,p)){
            printf("\nServer disconnected\n");
            return;
        }
        int packetID =getPacketID(p.header);
        if(packetID==PACKET_RESULT){
            if(p.length ==3 && memcmp(p.data,"END",3) == 0){
                printf("Cus display finish\n");
                return;
            }
            p.data[p.length]='\0';
            printf("%s\n",p.data);
        }
        else if(packetID == PACKET_CUSTOMER_ID){
            int id;
            memcpy(&id,p.data,sizeof(int));
            printf("%-10d ",id);
        }
        else if(packetID==PACKET_CUSTOMER_NAME){
            char name[50];
            int len=p.length;
            if(len>=sizeof(name))
            len=sizeof(name)-1;
            memcpy(name,p.data,len);
            name[len]='\0';
            printf("%-20s ",name);
        }
        else if(packetID==PACKET_CUSTOMER_ADDRESS){
            char address[100];
            int len=p.length;
            if(len>=sizeof(address))
            len=sizeof(address)-1;
            memcpy(address,p.data,len);
            address[len]='\0';
            printf("%-20s\n",address);
        }
        else{
            printf("Unknown Packet Id: %d\n",packetID);
        }
    }
    printf("\n----------------------------------------------------\n");
}
void displaySale(SOCKET client){
    printf("\n========SALE DETAILS========\n");
    printf("%-10s %-12s %-12s %-12s %-15s\n","SALE.ID","EMP.ID","CUS.ID","AMOUNT","DATE");
    printf("----------------------------------------------------\n");
    Packet p;
    int saleId=0;
    int cusId=0;
    int empId=0;
    float amount=0;
    char date[26];
    while(true){
        if(!receivePacket(client,p)){
            printf("\nServer disconnected\n");
            return;
        }
        int packetID =getPacketID(p.header);
        if(packetID==PACKET_RESULT){
            if(p.length == 3 && memcmp(p.data,"END",3) == 0){
                printf("Sale display finish\n");
                return;
            }
            p.data[p.length]='\0';
            printf("%s\n",p.data);
        }
        else if(packetID== PACKET_SALE_ID){
            int id;
            memcpy(&id,p.data,sizeof(int));
            printf("%-10d ",id);
        }
        else if(packetID== PACKET_SALE_EMP_ID){
            int empId;
            memcpy(&empId,p.data,sizeof(int));
            printf("%-12d ",empId);
        }
        else if(packetID == PACKET_SALE_CUS_ID){
            int cusId;
            memcpy(&cusId,p.data,sizeof(int));
            printf("%-12d ",cusId);
        }
        else if(packetID==PACKET_SALE_AMOUNT){
            float amount;
            memcpy(&amount,p.data,sizeof(float));
            printf("%-12.2f ",amount);
        }
        else if(packetID==PACKET_SALE_DATE){
            char date[50];
            int len=p.length;
            if(len>=sizeof(date))
            len=sizeof(date)-1;
            memcpy(date,p.data,len);
            date[len]='\0';
            printf("%-15s\n",date);
        }
        else{
            printf("Unknown Packet Id: %d\n",packetID);
        }
    }
    printf("\n----------------------------------------------------\n");
}
void viewAll(SOCKET client){
    int type;
    printf("\n1. Employee");
    printf("\n2. Customer");
    printf("\n3. Sale");
    printf("\nEnter View Type: ");
    scanf("%d",&type);
    if(type<1 || type>3){
        printf("Invalid Type\n");
        return;
    }
    unsigned char t=(unsigned char)type;
    if(!(sendPacket(client,PACKET_VIEW_ALL,1,FRAME_SINGLE,&t))){
        printf("Server Disconnected\n");
        return;
    }
    if(type==1){
        displayEmp(client);
    }
    else if(type==2){
        displayCus(client);
    }
    else if(type==3){
        displaySale(client);
    }
    else{
        printf("Invalid type\n");
    }
}
void deleteEmp(SOCKET client){
    int id;
    printf("\n===========================\n");
    printf("      DELETE EMPLOYEEE\n");
    printf("\n===========================\n");
    printf("Enter Employee Id: ");
    scanf("%d",&id);
    unsigned char data[5];
    data[0]=1;
    memcpy(data+1,&id,sizeof(int));
    sendPacket(client,PACKET_DELETE,5,FRAME_SINGLE,data);
    Packet p;
    if(!receivePacket(client,p)){
        printf("Server disconnected\n");
        return;
    }
    if(getPacketID(p.header)==PACKET_RESULT){
        char result[256];
        memcpy(result,p.data,p.length);
        result[p.length]='\0';
        printf("\nServer : %s\n",result);
    }
}
void deleteCus(SOCKET client){
    int id;
    printf("\n===========================\n");
    printf("      DELETE CUSTOMER\n");
    printf("\n===========================\n");
    printf("Enter Customer Id: ",id);
    scanf("%d",&id);
    unsigned char data[5];
    data[0]=2;
    memcpy(data+1,&id,sizeof(int));
    sendPacket(client,PACKET_DELETE,5,FRAME_SINGLE,data);
    Packet p;
    if(!receivePacket(client,p)){
        printf("Server disconnected\n");
        return;
    }
    if(getPacketID(p.header)==PACKET_RESULT){
        char result[256];
        memcpy(result,p.data,p.length);
        result[p.length]='\0';
        printf("\nServer : %s\n",result);
    }
}
void deleteSale(SOCKET client){
    int id;
    printf("\n===========================\n");
    printf("      SALE EMPLOYEEE\n");
    printf("\n===========================\n");
    printf("Enter Employee Id: ",id);
    scanf("%d",&id);
    unsigned char data[5];
    data[0]=3;
    memcpy(data+1,&id,sizeof(int));
    sendPacket(client,PACKET_DELETE,5,FRAME_SINGLE,data);
    Packet p;
    if(!receivePacket(client,p)){
        printf("Server disconnected\n");
        return;
    }
    if(getPacketID(p.header)==PACKET_RESULT){
        char result[256];
        memcpy(result,p.data,p.length);
        result[p.length]='\0';
        printf("\nServer : %s\n",result);
    }
}
void deleteData(SOCKET client){
    int type;
    printf("\n1. Employee");
    printf("\n2. Customer");
    printf("\n3. Sale");
    printf("\nEnter Delete Type: ");
    scanf("%d",&type);
    if(type==1){
        deleteEmp(client);
    }
    else if(type==2){
        deleteCus(client);
    }
    else if(type==3){
        deleteSale(client);
    }
    else{
        printf("Invalid type\n");
    }
}
void searchEmp(SOCKET client){
    char name[50]={0};
    printf("\n=========================================\n");
    printf("\n         SEARCH EMPLOYEE\n");
    printf("\n=========================================\n");
    printf("Enter Employee Name: ");
    scanf_s(" %49[^\n]",name,(unsigned)_countof(name));
    sendPacket(client,PACKET_SEARCH,strlen(name),FRAME_SINGLE,name);
    while(1){
        Packet p;
        if(!receivePacket(client,p)){
            printf("Server disconnected\n");
            return;
        }
        int packetId=getPacketID(p.header);
        if(packetId==PACKET_RESULT){
            printf("\nServer :%s\n",p.data);
            break;
        }
        else if(packetId==PACKET_EMPLOYEE_ID){
            int id;
            memcpy(&id,p.data,sizeof(int));
            printf("\nEmployee Id: %d\n",id);
        }
        else if(packetId==PACKET_EMPLOYEE_NAME){
            char name[50];
            memcpy(name,p.data,p.length);
            name[p.length]='\0';
            printf("\nEmployee Name: %s\n",name);
        }
        else if(packetId==PACKET_EMPLOYEE_SALARY){
            float salary;
            memcpy(&salary,p.data,sizeof(float));
            printf("\nEmployee Salary: %.2f\n",salary);
        }
        else if(packetId==PACKET_SALE_ID){
            int saleId;
            memcpy(&saleId,p.data,sizeof(int));
            printf("\n-------SALE-------\n");
            printf("\nSale Id: %d\n",saleId);
        }
        else if(packetId == PACKET_SALE_EMP_ID){
            int id;
            memcpy(&id,p.data,sizeof(int));
            printf("ID: %d\n", id);
        }
        else if(packetId == PACKET_SALE_CUS_ID){
            int cusId;
            memcpy(&cusId,p.data,sizeof(int));
            printf("Customer Id: %d\n",cusId);
        }
        else if(packetId == PACKET_SALE_AMOUNT){
            float amount;
            memcpy(&amount,p.data,sizeof(float));
            printf("Amount: %.2f\n", amount);
        }
        else if(packetId == PACKET_SALE_DATE){
            char date[26];
            memcpy(date,p.data,p.length);
            date[p.length] = '\0';
            printf("Date: %s\n",date);
        }
        else{
            printf("Unknown Packet Id: %d\n",packetId);
        }
    }
}
DWORD WINAPI clientThread(LPVOID lpParam){
    clientdata *info=(clientdata*)lpParam;
    SOCKET client=info->client;
    int clientid=info->clientid;
    printf("\nCLIENT %d THREAD STARTED\n",clientid);
    jsonConfig(client,clientid);
    int choice;
    while(true){
        printf("\n================MENU================\n");
        printf("1. Search\n");
        printf("2. Delete\n");
        printf("3. View All\n");
        printf("4. Exit\n");
        printf("Enter choice: ");
        scanf_s("%d",&choice);
        if(choice==1){
            searchEmp(client);
        }
        else if(choice==2){
            deleteData(client);
        }
        else if(choice==3){
            viewAll(client);
        }
        else if(choice==4){
            printf("Exiting\n");
            break;
        }
        else{
            printf("Invalid choice\n");
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
    loadJson();
    int clientNo;
    printf("Enter client No: ");
    scanf("%d",&clientNo);
    HANDLE hThread[10];
    int threadCount=0;
    for(int i=0;i<clientNo && i<10;i++){
        int clientid=i+1;
        SOCKET client=socket(AF_INET,SOCK_STREAM,0);
        if(client==INVALID_SOCKET){
            printf("Socket Creation failed for Client\n");
            continue;
        }
        sockaddr_in server;
        memset(&server,0,sizeof(server));
        server.sin_family = AF_INET;
        server.sin_port = htons(PORT);
        server.sin_addr.s_addr =inet_addr("127.0.0.1");
        if(connect(client,(sockaddr*)&server,sizeof(server)) == SOCKET_ERROR){
            printf("\nClient connection failed\n");
            closesocket(client);
            continue;
        }
        printf("Client %d connected successfully\n",clientid);
        clientdata *info=new clientdata;
        info->client=client;
        info->clientid=clientid;
        HANDLE thread=CreateThread(NULL,0,clientThread,(LPVOID)client,0,NULL);
        if(thread==NULL){
            printf("Client thread created failed\n");
            continue;
        }
        hThread[threadCount++]=thread;
        printf("CLIENT THREAD CREATED SUCCESSFULLY");
        for(int i=0;i<threadCount;i++){
            CloseHandle(hThread[i]);
        }
    }
    WSACleanup();
    return 0;
}
