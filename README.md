# concurrentStockServer_Project
이 프로젝트에서는 concurrentStockServer를 구현한다. 동시에 수 많은 client 들이 하나의 server에 접속해 주식 거래를 진행 할 수 있게 server를 구현하는 것이 중점이다.
이때 Readers-Writers 문제가 발생하지 않도록 Event-Based Server 방식과 Thread-Based Server 방식으로 나누어 구현한다. 

## project_1 : Event-Based Server
Select() 함수를 이용한 Event-Based Server 구현. 

### How to compile
```
> make
```
### How to run
#### client
```
> ./multiclient <hostname> <port> <client 개수>
```
#### server
```
> ./stockserver <port>
```

## project_2 : Thread-Based Server
Semaphore, Shared Buffer, Thread 를 이용한 Thread-Based Server 구현.

### How to compile
```
> make
```
#### client
```
> ./multiclient <hostname> <port> <client 개수>
```
#### server
```
> ./stockserver <port>
```
