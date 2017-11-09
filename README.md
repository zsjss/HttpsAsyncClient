# HttpsAsyncClient
c++ client http(s) sync/async

最近版本升级，需要支持https通信，用curl,openssl，自己编译了一个静态的lib;
实现过程参照curl文档 https://curl.haxx.se/libcurl/c/cacertinmem.html，生成证书这个过程被坑了好久，有时间上简书分享整个过程;
同步实现比较简单，异步的过程也是参照 https://github.com/samuelben/async_http_client 大神的思路;
经项目测验完美收官，有疑惑欢迎发email：625380963@qq.com;
