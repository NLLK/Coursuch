#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdbool.h>
#define COLOR_Green 32
#define COLOR_White 37
#define COLOR_Crimson 35
int numberOfTalons=0;
int windows[5]={0,0,0,0,1};
//int windows[5]={0,1,1,1,1};
//int seats[10]={0,0,0,0,0,0,0,0,0,0};
int seats[10]={0,0,0,0,0,0,0,0,0,0};
int fieldWidth=66;//ширина, 36+5*width =>36,41,46...
int fieldHeight=25;//высота

int freeWindow, WinN=0;
pthread_mutex_t mtxWinWrite,mtxWinRead;
sem_t semWinWrite, semWinRead;

int freeSeat, SeatN=0;
pthread_mutex_t mtxSeatWrite, mtxSeatRead;
sem_t semSeatWrite,semSeatRead;

pthread_t tid[20];//нити
void PrintInPoint(int x, int y, char* string)
{//печать в строке
	printf("\033[%d;%dH%s\n",y,x, string);
}
void ChangeColor(int color)
{
	printf("\033[0;%d;48m\n",color);
}
void DrawBorder(int x1, int y1, int x2, int y2)
{//печать рамки
	//x,y ╔══╗ X,y
	//	  ║  ║
	//x,Y ╚══╝ X,Y
	PrintInPoint(x1,y1,"╔");//угол 1
	PrintInPoint(x2,y1,"╗");//угол 2
	PrintInPoint(x1,y2,"╚");//угол 3
	PrintInPoint(x2,y2,"╝");//угол 4
	if ((x2-x1 != 1) && (y2 - y1 != 1))
	{
	for (int x=x1+1;x<=x2-1;x++)//верхняя и нижняя границы
	{
		PrintInPoint(x,y1,"═"); 
		PrintInPoint(x,y2,"═");
	}
	
	for(int y=y1+1; y<=y2-1;y++)//правая и левая границы
	{
		PrintInPoint(x1, y,"║");
		PrintInPoint(x2, y,"║");
	}
	}
}
void DrawFieldBorder()
{//рисовать рамку поля - здания
	DrawBorder(1,1,fieldWidth,fieldHeight);//рамка поля - здания
	PrintInPoint(fieldWidth/2-1,fieldHeight,"╝ ╚");//двери
}
void DrawSeats()
{//рисовать сиденья
	int fW = fieldWidth;
	ChangeColor(COLOR_Crimson);
	int x = 2;
	int y = 11;
	for(int n=0;n<4;n++)
	{//сиденья справа и слева
		PrintInPoint(x,y+n*3,  "╔═");
		PrintInPoint(x,y+1+n*3,"║");
		PrintInPoint(x,y+2+n*3,"╚═");
		PrintInPoint(x+fW-2*x,y+n*3,   "═╗");
		PrintInPoint(x+fW-2*x+1,y+1+n*3,"║");
		PrintInPoint(x+fW-2*x,y+2+n*3, "═╝");
	}
	for(int n=0;n<2;n++)
	{
		PrintInPoint(2+x+n*4,y+4*3,  "║  ║");
		PrintInPoint(2+x+n*4,y+4*3+1,"╚══╝");
	}
	ChangeColor(COLOR_White);
}
void DrawWindowsState()
{//рисовать окна
	int width=2+(fieldWidth-6)/5;//ширина
	int height=6;//высота
	for (int count=0; count<=4;count++)
	{
		char str[]="Okno 1";
		str[5]=count+1+'0';
		ChangeColor(COLOR_White);
		PrintInPoint(count*(width-1)+2, 2, str);
		PrintInPoint(count*(width-1)+2, 3, "Talon:");
		PrintInPoint(count*(width-1)+2, 4, "Vremya:");
		ChangeColor(COLOR_Green);
		DrawBorder(1+count*(width-1),1,width+count*(width-1),height);//отрисовка окна
		if (count>0)
		{
			PrintInPoint(1+count*(width-1),1,"╦");//сгладить линии между окнами сверху
			PrintInPoint(1+count*(width-1),height,"╩");//сгладить линии между окнами снизу
		}
	}
	PrintInPoint(1,height,"╠");//сгладить линии между окном и рамкой слева
	PrintInPoint(fieldWidth,height,"╣");//сгладить линии между окном и рамкой справа
}
int GetTimeForOperation(int op)
{//возвращает время для операции - 3 секунды для снятия наличных и 5 для других операций
	return (op==0)? 3: 5 ;
}
void* ChangeWindowTime(void* arg)
{
	int* send = (int*)arg;
	int window=send[0];
	int operation=send[1];
	int talon = send[2];
	char* tal=malloc(sizeof(char)*3);
	tal[0]=talon/10+'0'; tal[1]=talon%10+'0';tal[2]='\0';

	int width=2+(fieldWidth-6)/5;//ширина окна
	int time = GetTimeForOperation(operation);
	PrintInPoint((window-1)*(width-1)+9,3,tal);
	for (char t='0'; t<='0'+time; t++)
	{
		PrintInPoint((window-1)*(width-1)+10,4,&t);
		usleep(1000000);//секунду спит
	}
	PrintInPoint((window-1)*(width-1)+10,4," ");
	PrintInPoint((window-1)*(width-1)+9,3, "  ");
}
void DrawTalonMachine()
{//рисовать автомат по выдаче талонов
	int fW = fieldWidth;
	int fH = fieldHeight;
	ChangeColor(COLOR_Green);
	int width=7;//
	int height=4;
	int shiftH=5;
	int x1 = fW/2 - width/2;
	int y1 = fH/2 - height/2;
	int x2 = fW/2 + width/2;
	int y2 = fH/2 + height/2-1;
	DrawBorder(x1,y1+shiftH,x2,y2+shiftH);
	PrintInPoint(x1+1,y1+1+shiftH,"Talo-");
	PrintInPoint(x1+1,y1+2+shiftH,"ni");
	ChangeColor(COLOR_White);
}
void DrawTable()
{
	int width = 21;
	int stX = fieldWidth+1;
	DrawBorder(stX,1,stX+width,4);
	DrawBorder(stX,1,stX+width,fieldHeight);
	PrintInPoint(stX+1,2,"Posetiteli");
	PrintInPoint(stX+1,3,"Imya");
	PrintInPoint(stX+1+5,3,"Tal.");
	PrintInPoint(stX+1+5+5,3, "Oper.");
	PrintInPoint(stX+1+5+5+6,3, "Okno");
}
void WriteInTable(char* name, int talon, int operation, int window)
{
	int stX = fieldWidth+1;
	PrintInPoint(stX+1,4+talon, name);
	char* tal=malloc(sizeof(char)*3);
	tal[0]=talon/10+'0'; tal[1]=talon%10+'0';tal[2]='\0';
	char* op=malloc(sizeof(char)*2);
	op[0]=operation+'0';	op[1]='\0';
	char* win=malloc(sizeof(char)*2);
	win[0]=window+'0';	win[1]='\0';
	PrintInPoint(stX+1+5,4+talon,tal);
	PrintInPoint(stX+1+5+5,4+talon, op);
	PrintInPoint(stX+1+5+5+6,4+talon, win);
}
void ClearRow(int talon)
{
	int stX = fieldWidth+1;
	for (int i=0;i<20;i++)	
		PrintInPoint(stX+1+i,4+talon," ");
}
void DrawField()
{//рисовать поле
	ChangeColor(COLOR_Green);
	DrawFieldBorder();
	DrawWindowsState();
	DrawTalonMachine();
	ChangeColor(COLOR_White);
	DrawSeats();
	DrawTable();
}
int GetTalon()
{//получить талон
	numberOfTalons++;
	return numberOfTalons;
}
void GetWindow()
{//получить свободное окно
	freeWindow=0;
	for(int i=0; i<5;i++)
	{
		if (windows[i]==0)
		{
			freeWindow=i+1;
			break;
		}	
	}
}
void SetWindow()
{
	windows[freeWindow-1]=1;
}
void FreeAWindow(int window)
{
	windows[window-1]=0;
}
void GetSeat()
{//получить свободное кресло
	freeSeat = 0;
	for (int i = 0; i<10;i++)
	{
		if (seats[i] == 0)
		{
			freeSeat = i+1;
			return;
		}
	}	
}
void SetSeat()
{
	seats[freeSeat-1]=1;
}
void FreeASeat(int seat)
{
	seats[seat-1]=0;
}
void GoFromTo(char* name, int x1, int y1, int x2, int y2, int how)
{//перемещение из точки в точку
	//how - задает как идти к точке 
	//четное - по у потом по х. иначе - наоборот
	PrintInPoint(x1,y1, " ");
	int sign;
	int count=0;
	for (int i=how; i<=how+1;i++)
	{
		int from=0;
		int to=0;
		if (i%2==0)
		{ 	//идти по y
			sign = (y2>=y1)?1:-1;
			from = (count==0)?x1:x2;
			for (int y=y1; y!=y2; y+=sign)
			{
				if(y!=y1)	PrintInPoint(from, y-sign, " ");
				PrintInPoint(from, y,name);
				usleep(200000);//лучше случайное сделать
			}
			if (y1!=y2) PrintInPoint(from, y2-sign, " ");
		}
		else
		{	//идти по х
			sign = (x2>=x1)?1:-1;
			from = (count==0)?y1:y2;

			for (int x = x1; x!=x2; x+=sign)
			{
				if (x!=x1)	PrintInPoint(x-sign, from, " ");
				PrintInPoint(x, from,name);
				usleep(200000);
			}
			if (x1!=x2)	PrintInPoint(x2-sign, from, " ");
		}
		count++;
		DrawField();
	}
	PrintInPoint(x2,y2,name);
}
int* CreateArray(int a, int b, int c)
{
	int *send = malloc(sizeof(int)*3);
	send[0]=a;	send[1]=b;	send[2]=c;
	return send;
}
void AbstractReader(void* mtxRead, void* mtxWrite, void* semRead, void* semWrite, int* N, void* reader, int mode)
{
	void (*program)() = reader;
	bool ex = false;
	sem_wait(semRead);
	do{
		int rc = pthread_mutex_trylock(mtxRead);
		if (rc == 0)
		{
			pthread_mutex_lock(mtxWrite);
			*N+=1;
			program();

			for(int i=0;i<5;i++)
			{	
				char* a= malloc(sizeof(char));
				a[0]=windows[i]+'0';
				PrintInPoint(90,i+1,a);
			}
			for(int i=0;i<5;i++)
			{	
				char* a= malloc(sizeof(char));
				a[0]=seats[i]+'0';
				PrintInPoint(90,i+10,a);
			}
			pthread_mutex_unlock(mtxWrite);
			pthread_mutex_unlock(mtxRead);
			sem_post(semRead);
			if ((mode == 1) && (freeWindow == 0)) ex=false;//на случай, если нужно ждать не нулевого значения 
			else ex=true;
		}
	}
	while (ex == false);
}
void AbstractWriter(void* mtxWrite, void* semRead, void* semWrite, void* writer)
{
	void (*program)() = writer;
	bool ex = false;
	do
	{
		//sem_wait(semWrite);
		int rc=-1;
		rc = pthread_mutex_trylock(mtxWrite);
		if (rc==0)
		{
			program();//функция
			ex=true;
			//sem_post(semRead);
			pthread_mutex_unlock(mtxWrite);
		}
	}while (ex==false);
}
void* visitor(void *arg)
{
	char received = (char)arg;//получение имени из пришедшей информации
	char* name=malloc(sizeof(char));
	name[0]=received;
	//спим некоторое время перед тем как войти
	usleep((random()%7)*1000000);
	//идем
	int hereX=33, hereY=25;//из дверей
	GoFromTo(name,33,25,33,19,0);//к автомату
	hereX = 33; hereY=19;//теперь мы у автомата
	int operation = random()%2;//опеределяем операцию, ради которой пришли

	//подождать, если кто-то юзает талонный автомат()

	int talon = GetTalon();//получили талон

	int window = 0;

	AbstractReader(&mtxWinRead, &mtxWinWrite, &semWinRead, &semWinWrite, &WinN, &GetWindow,0);//определили свободное окно
	window=freeWindow;//запомнили
	AbstractWriter(&mtxWinWrite, &semWinRead, &semWinWrite, &SetWindow);//записали его как занятое

	usleep(1000000);//немного постоим, подождем пока дадут талон

	WriteInTable(name,talon,operation,window);//записали все это в таблицу посетителей

	int seat;
	if (window==0) //если окон свободных нет - сесть посидеть и ждать пока освободится окно
	{
		//определить свободное место
		AbstractReader(&mtxSeatRead, &mtxSeatWrite, &semSeatRead, &semSeatWrite, &SeatN, &GetSeat,0);
		seat = freeSeat;
		AbstractWriter(&mtxSeatWrite, &semSeatRead, &semSeatWrite, &SetSeat);
		int x,y;
		if (seat <=4)
		{
			x=3;
			y=11+1+(seat-1)*3;
		}
		else if (seat<=8)
		{
			x=fieldWidth-2;
			y=11+1+(seat-5)*3;
		}
		//определили где сесть
		GoFromTo(name,hereX,hereY,x,y,0);
		hereX=x; hereY=y;//теперь мы тут, на сиденьях

		//ждем, пока окно не освободится
		AbstractReader(&mtxWinRead, &mtxWinWrite, &semWinRead, &semWinWrite, &WinN, &GetWindow,1);
		window=freeWindow;
		AbstractWriter(&mtxWinWrite, &semWinRead, &semWinWrite, &SetWindow);//записали его как занятое
		FreeASeat(seat);//освободить сидушку
		WriteInTable(name,talon,operation,window);//записали все это в таблицу посетителей
	}
	//если есть свободные окна - идти в него
	int width = 1+(fieldWidth-6)/5;
	int x = (window-1)*(width)+width/2;
	int y = 7;
	GoFromTo(name,hereX,hereY,x,y,1);
	hereX=x; hereY=y;//теперь мы возле окна

	//запуск нити для изменения времени
	pthread_t id;
	int *send = CreateArray(window,operation,talon);//создаем массив из окна и операции, чтобы передать это в поток
	pthread_create(&id, NULL, (void*)ChangeWindowTime, (void*)send);
	usleep(GetTimeForOperation(operation)*1000000);//спим столько же, сколько и меняется время в окне
	FreeAWindow(window);//освобождаем окно
	ClearRow(talon);//удаляем себя из таблицы
	GoFromTo(name,hereX,hereY,33,20,0);//идем на выход, до автомата(почти)
	GoFromTo(name,33,20,33,25,1);//идем на выход окончательно
	usleep(200000);//перед тем как выйти еще чуть чуть существуем в дверях
	PrintInPoint(33,25," ");//удаляемся окончательно
}
void MutexsSemsInit()
{//инициализация мютексов и семафоров
	pthread_mutex_init(&mtxWinRead, NULL);//
	pthread_mutex_init(&mtxWinWrite, NULL);//
	sem_init(&semWinWrite, 1, 1);//
	sem_init(&semWinRead, 1, 1);//
	sem_post(&semWinRead);

	pthread_mutex_init(&mtxSeatRead, NULL);//
	pthread_mutex_init(&mtxSeatWrite, NULL);//
	sem_init(&semSeatWrite, 1, 1);//
	sem_init(&semSeatRead, 1, 1);//
	sem_post(&semSeatRead);
}
void main()
{
	printf("\033[H\033[2J");//clear screen
	DrawField();
	MutexsSemsInit();
	for (char name='A';name<'A'+20; name++)
	{
		int rc=	pthread_create(&tid[name-'A'],NULL, (void*)visitor, (void*)name);
		usleep(2000000);
	}
	getchar();
}