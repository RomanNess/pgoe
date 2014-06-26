
#include <cstdio>
void print()__attribute__((noinline));
void a(int i)__attribute__((noinline));
void b()__attribute__((noinline));

void print(){
	printf("Hello world");
}

void a(int i){
	if(i==0)
		print();
}

void b(){
	print();
}

int main(int argc, char** argv){

	printf("Little test generating call conjunction.\n");

	for(int i=0; i<1000000; i++) {
		a(i);
	}
	b();

return 0;
}
