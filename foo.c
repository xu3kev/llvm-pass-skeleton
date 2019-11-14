#include<stdio.h>
int mul(int x){
    for(int i=0;i<3;++i){
        x = x*3;
    }
    return x;
}
int main(){
    int a = mul(5);
    printf("%d\n",a);
    return 0;
}
