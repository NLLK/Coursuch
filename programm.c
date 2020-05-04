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
int numberOfTalons=0;//кол-во талонов в системе
int windows[5]={0,0,0,0,0};//состояние окон
int seats[8]={0,0,0,0,0,0,0,0};//состояние сидений
int fieldWidth=66;//ширина, 36+5*width =>36,41,46...
int fieldHeight=25;//высота

int freeWindow;//пустое окно
pthread_mutex_t mtxWinWrite,mtxWinRead;//мютексы для записи и чтения с windows[]
sem_t semWinRead;//семафор чтения с windows[]

int freeSeat;//пустое кресло
pthread_mutex_t mtxSeatWrite, mtxSeatRead;//мютексы для записи и чтения с seats[]
sem_t semSeatRead;//семафор чтения с seats[]

pthread_t tid[20];//нити посетителей
void PrintInPoint(int x, int y, char* string)
{//печать в строку. требует координаты х,у, и строку
	printf("\033[%d;%dH%s\n",y,x, string);
}
void ChangeColor(int color)
{//смена цвета, требует числовое значение цвета
	printf("\033[0;%d;48m\n",color);
}
void DrawBorder(int x1, int y1, int x2, int y2)
{//печать рамки. Требует координаты двух диоганальных точек
	//x,y ╔══╗ X,y
	//	  ║  ║
	//x,Y ╚══╝ X,Y
	ChangeColor(COLOR_Green); PrintInPoint(x1,y1,"╔");//угол 1
	ChangeColor(COLOR_Green); PrintInPoint(x2,y1,"╗");//угол 2
	ChangeColor(COLOR_Green); PrintInPoint(x1,y2,"╚");//угол 3
	ChangeColor(COLOR_Green); PrintInPoint(x2,y2,"╝");//угол 4
	if ((x2-x1 != 1) && (y2 - y1 != 1))//в случае, если это квадрат 1*1 то все уже было выведено
	{
	for (int x=x1+1;x<=x2-1;x++)//верхняя и нижняя границы
	{
		ChangeColor(COLOR_Green);	PrintInPoint(x,y1,"═"); 
		ChangeColor(COLOR_Green);	PrintInPoint(x,y2,"═");
	}
	
	for(int y=y1+1; y<=y2-1;y++)//правая и левая границы
	{
		ChangeColor(COLOR_Green);	PrintInPoint(x1, y,"║");
		ChangeColor(COLOR_Green);	PrintInPoint(x2, y,"║");
	}
	}
}
void DrawFieldBorder()
{//рисовать рамку поля - здания
	ChangeColor(COLOR_Green);	DrawBorder(1,1,fieldWidth,fieldHeight);//рамка поля - здания
	ChangeColor(COLOR_Green);	PrintInPoint(fieldWidth/2-1,fieldHeight,"╝ ╚");//двери
}
void DrawSeats()
{//рисовать сиденья
	int x = 2;//х первого сиденья
	int y = 11;//у первого сиденья
	for(int n=0;n<4;n++)
	{//сиденья справа и слева
		ChangeColor(COLOR_Crimson);	PrintInPoint(x,y+n*3,  "╔═");
		ChangeColor(COLOR_Crimson);	PrintInPoint(x,y+1+n*3,"║");
		ChangeColor(COLOR_Crimson);	PrintInPoint(x,y+2+n*3,"╚═");
		ChangeColor(COLOR_Crimson);	PrintInPoint(x+fieldWidth-2*x,y+n*3,   "═╗");
		ChangeColor(COLOR_Crimson);	PrintInPoint(x+fieldWidth-2*x+1,y+1+n*3,"║");
		ChangeColor(COLOR_Crimson);	PrintInPoint(x+fieldWidth-2*x,y+2+n*3, "═╝");
	}
	for(int n=0;n<2;n++)
	{//сиденья снизу
		ChangeColor(COLOR_Crimson); PrintInPoint(2+x+n*4,y+4*3,  "║  ║");
		ChangeColor(COLOR_Crimson);	PrintInPoint(2+x+n*4,y+4*3+1,"╚══╝");
	}
	ChangeColor(COLOR_White);
}
void DrawWindowsState()
{//рисовать окна
	int width=2+(fieldWidth-6)/5;//ширина
	int height=6;//высота
	for (int count=0; count<=4;count++)//для 5-ти окон
	{
		char str[]="Okno 1";//заготовка под строку "окно"
		str[5]=count+1+'0';//меняем цифру на нужную
		ChangeColor(COLOR_White);	PrintInPoint(count*(width-1)+2, 2, str);
		ChangeColor(COLOR_White);	PrintInPoint(count*(width-1)+2, 3, "Talon:");
		ChangeColor(COLOR_White);	PrintInPoint(count*(width-1)+2, 4, "Vremya:");
		ChangeColor(COLOR_Green);	DrawBorder(1+count*(width-1),1,width+count*(width-1),height);//отрисовка окна
		if (count>0)
		{
			ChangeColor(COLOR_Green);	PrintInPoint(1+count*(width-1),1,"╦");//сгладить линии между окнами сверху
			ChangeColor(COLOR_Green);	PrintInPoint(1+count*(width-1),height,"╩");//сгладить линии между окнами снизу
		}
	}
	ChangeColor(COLOR_Green);	PrintInPoint(1,height,"╠");//сгладить линии между окном и рамкой слева
	ChangeColor(COLOR_Green);	PrintInPoint(fieldWidth,height,"╣");//сгладить линии между окном и рамкой справа
}
int GetTimeForOperation(int op)
{//возвращает время для операции - 3 секунды для снятия наличных и 5 для других операций
	return (op==0)? 3: 5 ;
}
void* ChangeWindowTime(void* arg)
{//меняет время и номер талона, когда посетитель подходит к окну
	int* send = (int*)arg;//принятая информация
	int window=send[0];//окно
	int operation=send[1];//операция
	int talon = send[2];//номер талона
	char* tal=malloc(sizeof(char)*3);//переводим номер талона в строку
	tal[0]=talon/10+'0'; tal[1]=talon%10+'0';tal[2]='\0';

	int width=2+(fieldWidth-6)/5;//ширина окна
	int time = GetTimeForOperation(operation);//время для операции
	PrintInPoint((window-1)*(width-1)+9,3,tal);//вывод талона
	for (char t='0'; t<='0'+time; t++)//для отсчета времени - от 0 до t
	{
		ChangeColor(COLOR_White);	PrintInPoint((window-1)*(width-1)+10,4,&t);//вывод секунды
		usleep(1000000);//секунду спит
	}
	PrintInPoint((window-1)*(width-1)+10,4," ");//очистка
	PrintInPoint((window-1)*(width-1)+9,3, "  ");
}
void DrawTalonMachine()
{//рисовать автомат по выдаче талонов
	int width=7;//ширина автомата
	int height=4;//высота
	int shiftH=5;//сдвиг от центра экрана
	int x1 = fieldWidth/2 - width/2;//начальные координаты
	int y1 = fieldHeight/2 - height/2;
	int x2 = fieldWidth/2 + width/2;//конечные координаты
	int y2 = fieldHeight/2 + height/2-1;
	DrawBorder(x1,y1+shiftH,x2,y2+shiftH);//вывод коробки
	ChangeColor(COLOR_Green);	PrintInPoint(x1+1,y1+1+shiftH,"Talo-");//и текста
	ChangeColor(COLOR_Green);	PrintInPoint(x1+1,y1+2+shiftH,"ni");
}
void DrawTable()
{//отрисовка таблицы
	int width = 21;//ширина таблицы
	int stX = fieldWidth+1;//стартовая точка таблицы
	DrawBorder(stX,1,stX+width,4);//основа таблицы
	DrawBorder(stX,1,stX+width,fieldHeight);//шапка
	ChangeColor(COLOR_White);	PrintInPoint(stX+1,2,"Posetiteli");//вывод информации в шапку
	ChangeColor(COLOR_White);	PrintInPoint(stX+1,3,"Imya");
	ChangeColor(COLOR_White);	PrintInPoint(stX+1+5,3,"Tal.");
	ChangeColor(COLOR_White);	PrintInPoint(stX+1+5+5,3, "Oper.");
	ChangeColor(COLOR_White);	PrintInPoint(stX+1+5+5+6,3, "Okno");
}
void WriteInTable(char* name, int talon, int operation, int window)
{//писать в таблицу информацию. Требует имя, талон, операцию и окно
	int stX = fieldWidth+1;//начальная точка таблицы
	PrintInPoint(stX+1,4+talon, name);//вывод имени
	char* tal=malloc(sizeof(char)*3);//подготовка строки с талоном
	tal[0]=talon/10+'0'; tal[1]=talon%10+'0';tal[2]='\0';
	char* op=malloc(sizeof(char)*2);//подготовка строки с операцией
	op[0]= (operation==0)?'N':'D';//если 0 - снятие наличных, если 1 - другое 
	op[1]='\0';
	char* win=malloc(sizeof(char)*2);//подготовка строки с окном
	win[0]=window+'0';	win[1]='\0';
	ChangeColor(COLOR_White);	PrintInPoint(stX+1+5,4+talon,tal);//вывод талона
	ChangeColor(COLOR_White);   PrintInPoint(stX+1+5+5,4+talon, op);//операции
	ChangeColor(COLOR_White);   PrintInPoint(stX+1+5+5+6,4+talon, win);//окна
}
void ClearRow(int talon)
{//очистка строки в таблице. Требует номер талона
	int stX = fieldWidth+1;//стартовые координаты таблицы
	for (int i=0;i<20;i++)//вывод двадцати пробелов в нужной строке
		PrintInPoint(stX+1+i,4+talon," ");
}
void DrawField()
{//рисовать поле
	DrawFieldBorder();//границы - здание
	DrawWindowsState();//окна
	DrawTalonMachine();//автомат по выдаче талонов
	DrawSeats();//сиденья
	DrawTable();//таблица
}
int GetTalon()
{//получить талон
	numberOfTalons++;//колво талонов всего
	return numberOfTalons;//возвращает новый талон
}
void GetWindow()
{//получить свободное окно
	freeWindow=0;//свободное окно
	for(int i=0; i<5;i++)//перебираем все окна
	{
		if (windows[i]==0)//если окно свободное - вывод его в "свободное окно"
		{
			freeWindow=i+1;
			break;
		}	
	}
}
void SetWindow()
{//установитиьт окно как более не свободное
	windows[freeWindow-1]=1;//занимаем окно единицей
}
void FreeAWindow(int window)
{//освободить окною Требует номер окна
	windows[window-1]=0;//освобождаем окно нулем
}
void GetSeat()
{//получить свободное кресло
	freeSeat = 0;//свободное кресло
	for (int i = 0; i<8;i++)//для всех кресел
	{
		if (seats[i] == 0)//если кресло свободное - вывод его в "свободное кресло"
		{
			freeSeat = i+1;
			return;
		}
	}	
}
void SetSeat()
{//установить кресло как более не свободное
	seats[freeSeat-1]=1;//занимаем кресло единицей
}
void FreeASeat(int seat)
{//освободить кресло. Требует номер кресла
	seats[seat-1]=0;//освобождаем кресло нулем
}
void GoFromTo(char* name, int x1, int y1, int x2, int y2, int how)
{//перемещение из точки в точку
	//how - задает как идти к точке 
	//четное - по у потом по х. иначе - наоборот
	PrintInPoint(x1,y1, " ");
	int sign;//знак - в какую строну идем
	int count=0;//вспомогательная переменная для how. определяет по какой координате идти и куда уже шли
	for (int i=how; i<=how+1;i++)
	{
		int from=0;//откуда (по х и по у)
		if (i%2==0)//если четное - то сначала по у идем
		{ 	//идти по y
			sign = (y2>=y1)?1:-1;//задаем в какую сторону
			from = (count==0)?x1:x2;//и куда
			for (int y=y1; y!=y2; y+=sign)
			{
				if(y!=y1)	PrintInPoint(from, y-sign, " ");//затираем предыдущую точку
				ChangeColor(COLOR_White);	PrintInPoint(from, y,name);//пишем в точке имя
				usleep(200000);//ждем немного
			}
			if (y1!=y2) PrintInPoint(from, y2-sign, " ");//затираем предпоследнюю точку
		}
		else//иначе - по х
		{	//идти по х
			sign = (x2>=x1)?1:-1;//задаем в какую сторону
			from = (count==0)?y1:y2;//и куда
			for (int x = x1; x!=x2; x+=sign)
			{
				ChangeColor(COLOR_White);
				if (x!=x1)	PrintInPoint(x-sign, from, " ");//затираем предыдущую точку
				ChangeColor(COLOR_White);	PrintInPoint(x, from,name);//пишем в точке имя
				usleep(200000);//ждем немного
			}
			if (x1!=x2)	PrintInPoint(x2-sign, from, " ");//затираем предпоследнюю точку
		}
		count++;
		DrawField();//отрисовка поля, чтобы восстановить окружение
	}
	ChangeColor(COLOR_White);	PrintInPoint(x2,y2,name);//вывод в конце концов имени
}
int* CreateArray(int a, int b, int c)
{//вспомогательная функция. Возвращает ссылку на массив. Требудет три значение int
	int *send = malloc(sizeof(int)*3);//выделяем память
	send[0]=a;	send[1]=b;	send[2]=c;//записываем все в массив
	return send;//возвращаем ссылку
}
void AbstractReader(void* mtxRead, void* mtxWrite, void* semRead, void* reader, int mode)
{//абстрактный читатель. Для окон и сидений. Требует мютексы чтения, записи, семафор для чтения, функцию читатель, и режим
	//режим задает, нужно ли выходить из цикла, если получили любое значение - при 0, и при 1 - выйти, если не 0
	void (*program)() = reader;//получаем функцию
	bool ex = false;//переменная для опеределения выхода
	sem_wait(semRead);//ждем пока последний прочитает
	do{
		int rc = pthread_mutex_trylock(mtxRead);//по возможности блокируем доступ по чтению
		if (rc == 0)
		{
			pthread_mutex_lock(mtxWrite);//блокируем запись
			program();//читаем
			pthread_mutex_unlock(mtxWrite);//разблокируем запись
			pthread_mutex_unlock(mtxRead);//разблокируем чтение
			sem_post(semRead);//сообщаем, что закончили читать
			if ((mode == 1) && (freeWindow == 0)) ex=false;//на случай, если нужно ждать не нулевого значения 
			else ex=true;
		}
	}
	while (ex == false);//пока не прочитаем
}
void AbstractWriter(void* mtxWrite, void* writer)
{//абстрактный писатель. Для окон и сидений. Требует мютекс для записи и функцию писатель
	void (*program)() = writer;//получаем функцию писатель
	bool ex = false;//переменная для определения выхода
	do
	{
		int rc = pthread_mutex_trylock(mtxWrite);//пытаемся заблокировать мютекс по записи
		if (rc==0)//ждем, пока будет возможность записать
		{
			program();//записываем
			ex=true;//собираемся выходить
			pthread_mutex_unlock(mtxWrite);//разблокируем мютекс по записи
		}
	}while (ex==false);
}
void* visitor(void *arg)
{//посетитель. Требует имя пользователя в виде символа
	char received = (char)arg;//получение имени из пришедшей информации
	char* name=malloc(sizeof(char));
	name[0]=received;
	usleep((random()%7)*1000000);//спим некоторое время перед тем как войти
	//идем
	int hereX=33, hereY=25;//из дверей
	GoFromTo(name,33,25,33,19,0);//к автомату
	hereX = 33; hereY=19;//теперь мы у автомата
	int operation = random()%2;//опеределяем операцию, ради которой пришли
	int talon = GetTalon();//получили талон
	int window = 0;

	AbstractReader(&mtxWinRead, &mtxWinWrite, &semWinRead, &GetWindow,0);//определили свободное окно
	window=freeWindow;//запомнили
	AbstractWriter(&mtxWinWrite, &SetWindow);//записали его как занятое

	usleep(1000000);//немного постоим, подождем пока дадут талон

	WriteInTable(name,talon,operation,window);//записали все это в таблицу посетителей

	int seat;//сиденье
	if (window==0) //если окон свободных нет - сесть посидеть и ждать пока освободится окно
	{
		//определить свободное место
		AbstractReader(&mtxSeatRead, &mtxSeatWrite, &semSeatRead, &GetSeat,0);//прочитали свободное место
		seat = freeSeat;//запомнили свободное место
		AbstractWriter(&mtxSeatWrite, &SetSeat);//записали его как занятое
		int x,y;//координаты сиденья
		if (seat <=4)//высчитываем координаты в зависимости от стороны
		{
			x=3;
			y=11+1+(seat-1)*3;
		}
		else if (seat<=8)
		{
			x=fieldWidth-2;
			y=11+1+(seat-5)*3;
		}
		GoFromTo(name,hereX,hereY,x,y,0);//идем к сиденью
		hereX=x; hereY=y;//теперь мы тут, на сиденьях
		AbstractReader(&mtxWinRead, &mtxWinWrite, &semWinRead, &GetWindow,1);//ждем, пока окно не освободится
		window=freeWindow;//запомнили окно
		AbstractWriter(&mtxWinWrite,&SetWindow);//записали его как занятое
		FreeASeat(seat);//освободить сидушку
		WriteInTable(name,talon,operation,window);//записали все это в таблицу посетителей
	}
	//если есть свободные окна - идти в него
	int width = 1+(fieldWidth-6)/5;//высчитываем снова ширину окна
	int x = (window-1)*(width)+width/2;//высчитываем координаты нужного окна
	int y = 7;
	GoFromTo(name,hereX,hereY,x,y,1);//идем к окну
	hereX=x; hereY=y;//теперь мы возле окна

	//запуск нити для изменения времени
	pthread_t id;
	int *send = CreateArray(window,operation,talon);//создаем массив из окна и операции, чтобы передать это в поток
	pthread_create(&id, NULL, (void*)ChangeWindowTime, (void*)send);//меняем цифры времени и талона у окна
	usleep(GetTimeForOperation(operation)*1000000);//спим столько же, сколько и меняется время в окне
	FreeAWindow(window);//освобождаем окно
	ClearRow(talon);//удаляем себя из таблицы
	GoFromTo(name,hereX,hereY,33,20,0);//идем на выход, до автомата(почти)
	GoFromTo(name,33,20,33,25,1);//идем на выход окончательно
	usleep(200000);//перед тем как выйти еще чуть-чуть существуем в дверях
	PrintInPoint(33,25," ");//удаляемся окончательно
}
void MutexsSemsInit()
{//инициализация мютексов и семафоров
	pthread_mutex_init(&mtxWinRead, NULL);//мютекс для чтения окон
	pthread_mutex_init(&mtxWinWrite, NULL);//мютекс для записи окон
	sem_init(&semWinRead, 1, 1);//семафор для чтения окон
	sem_post(&semWinRead);//инициализируем семафор единицей

	pthread_mutex_init(&mtxSeatRead, NULL);//мютекс для чтения сидений
	pthread_mutex_init(&mtxSeatWrite, NULL);//мютекс для записи сидений
	sem_init(&semSeatRead, 1, 1);//семфор для чтения сидений
	sem_post(&semSeatRead);//инициализируем семафор единицей
}
void MutexsSemsDestroy()
{
	pthread_mutex_destroy(&mtxWinRead);
	pthread_mutex_destroy(&mtxWinWrite);
	pthread_mutex_destroy(&mtxSeatWrite);
	pthread_mutex_destroy(&mtxSeatRead);
	sem_destroy(&semWinRead);
	sem_destroy(&semSeatRead);
}
void main()
{//главная функция
	printf("\033[H\033[2J");//очищаем окно
	DrawField();//рисуем фон - поле
	MutexsSemsInit();//инициализируем мютексы и семафоры
	for (char name='A';name<'A'+20; name++)//для имен от А до T
	{
		int rc=	pthread_create(&tid[name-'A'],NULL, (void*)visitor, (void*)name);//создаем поток
		usleep(2000000);//дополнительно немного ждем
	}
	getchar();//ждем, пока будет нажата клавиша или ctrl+c
	MutexsSemsDestroy();
}