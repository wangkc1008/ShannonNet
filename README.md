# ShannonNet
## 公式
```
A 为双方共有的私钥
encrypt:
  b(M * 1) = A(M x N) * s(N x 1) + noise(M * 1) + m(M * 1) * Q / 2
  传输 b 和 s
decrypt:
  b_(M * 1) = b(M * 1) - A(M x N) * s(N x 1);
  b_ <  Q / 2 时 传输的为 0
  b_ >= Q / 2 时 传输的为1
```
## 预先安装 
[nginx](https://nginx.org/en/)<br>
[nginx配置文件](https://github.com/DPSpace/ShannonNet/blob/main/shannonnet.conf)<br>
[redis](https://redis.io/) <br>
[conan](https://docs.conan.io/en/latest/) <br>
## 编译
mkdir build && cd build<br>
conan install ..<br>
cmake ..<br>
make<br>
## 运行
目前已完成<b>单节点</b>*<b>CPU</b>预生成秘钥、加解密、http传输密钥和传输验证功能，多节点可同步扩展 <br>
### 执行文件
<b>按照以下顺序执行</b><br>
```
bin/shannonnet_init 项目初始化脚本: 创建目录、redis初始化、为各节点生成初始化秘钥.
		    每个秘钥都有一个唯一的索引index
		    1. running/{index}/{index}.bin                            存放与index对应的运行时秘钥.
		    2. secrets/node_{serverNodeId}_{clientNodeId}/{index}.bin 存放节点间通信秘钥.
		    3. waiting/{index}/{index}_waited.bin                     存放与index对应的待发送秘钥.
bin/generator	    秘钥生成脚本: 通过队列消息为本节点中的服务端生成running秘钥和waiting秘钥, 为客户端生成running秘钥.
bin/oatpp_server    服务端可执行文件
bin/oatpp_client    客户端可执行文件
```

### 项目初始化脚本<br>
init/init.cc<br>
```
	1. 创建秘钥存储目录.<br>
	2. 为各节点生成初始化秘钥, 存储在secrets/, 服务端-客户端下的秘钥文件相同.<br>
	3. 删除redis下的相关key并添加初始化数据.
```

### 秘钥生成脚本<br>
src/scripts/generateSecretFile.cc<br>
```
队列消息格式
{
	{
		"source", 
		"server/client"
	}, {
		"randomNum",
		123456789
	}, {
		"index",
		"aM96UXFs4WKKC7nY"
	}, {
		"serverNodeId",
		0
	}, {
		"clientNodeId",
		1
	}, {
		"value",
		""
	}
}
function:
	1. 接收、校验队列消息.
	2. 创建相关秘钥存储目录.
	3. 读取节点下的secrets/内的有效秘钥.
	4. 为每个有效秘钥文件生成固定数量( THREAD_NUM )个tmp秘钥文件, 输出 THREAD_NUM * FILE_NUM.
	5. 根据线程ID将n个有效秘钥文件生成的n个tmp秘钥 异或 合并为1个秘钥文件, 输出 THREAD_NUM 个秘钥文件.
	6. 将 THREAD_NUM 个秘钥文件合并为 1 个秘钥文件.
	7. 只有source = "server"的消息才会生成waiting秘钥.
	8. 设置必要的redis信息.
```

### 接口文件<br>
src/controller/SecretController.hpp<br>
1. /shannonnet/gen_secret/{nodeIdArg}/<br>
```
brief:
传输秘钥前client需要调用改接口生成传输过程所需秘钥

requset:
nodeIdArg       客户端节点ID  uint16

reqponse:
statusCode   状态码
description  描述信息
data
      nodeId     服务端节点ID             uint16
      randomNum  生成秘钥的随机数种子     uint32
      index      所有秘钥的全局唯一索引   string
 
example:
{
	"statusCode": 200,
	"description": "success",
	"data": {
		"nodeId": 0,
		"randomNum": 2072802693,
		"index": "3N7AOlmutc2662w6"
	}
}

function:
  1. 参数校验
  2. 生成随机数和index
  3. index检查和必要的锁机制
  4. push队列消息, 生成server秘钥
```
2. /shannonnet/get_data/{nodeIdArg}/{indexArg}/{progressArg}/<br>
```
brief:
传输秘钥接口

request:
nodeIdArg       客户端节点ID    uint16
indexArg        秘钥唯一索引值  string
progressArg     秘钥发送进度    uint32

response:
statusCode   状态码
description  描述信息
data
      secretS     公钥s   string
      secretB     公钥b   string
      progress    进度ID  uint32

example:
{
	"statusCode": 200,
	"description": "success",
	"data": [{
			"secretS": "......",
			"secretB": "......",
			"progress": 0
		},
           ......
		{
			"secretS": "......",
			"secretB": "......",
			"progress": 1023
		}
	]
}

function:
  1. 参数校验
  2. redis key及秘钥文件校验
  3. 秘钥传输进度校验
  4. 读取指定进度位置的秘钥内容, 加密, 累计EACH_NUM次, 每次S_LEN字节
  5. 根据redis中的进度记录判断当前秘钥文件是否传输完毕
  6. 锁机制及move秘钥文件
```
3. /shannonnet/result_report/{nodeIdArg}/{indexArg}/{isValidArg}/
```
brief:
秘钥传输完毕的结果上报接口

request:
nodeIdArg       客户端节点ID    uint16
indexArg        秘钥唯一索引值  string
isValidArg      秘钥有效值      uint16

response:
statusCode   状态码
description  描述信息
data

example:
{
	"statusCode": 200,
	"description": "success",
	"data": []
}

function:
  1. 参数校验
  2. redis key校验及秘钥文件校验
  3. valid = NODE_SECRET_VALID时修改redis key, 设置秘钥有效
  4. 否则 设置秘钥无效并删除server的秘钥文件
```

### 客户端文件 <br>
src/ClientApp.cc<br>
```
brief:
客户端秘钥传输接口文件

function:
  1. 判断是否要更新秘钥 (todo)
  2. 调预生成秘钥接口
  3. push队列消息, 生成client秘钥
  4. 循环判断server和client秘钥是否生成完毕
  5. 循环向server拉取秘钥, 解密并存储接收到的秘钥文件
  6. 接收完成时进行crc32校验, 成功时则上报有效结果并move秘钥文件
  7. 否则上报无效结果并删除client秘钥文件
```

## 配置 <br>
src/config/config.h
```
brief: 
全局配置文件

function:
包括脚本配置、密钥配置，服务端、客户端、日志和Redis等的基础配置.
```
