#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include<time.h>
#include<signal.h>

#define	READ	0      /* The index of the “read” end of the pipe */ 
#define	WRITE	1      /* The index of the “write” end of the pipe */ 

//structure to pass data
struct registrar_thread_params
{
	FILE *fp;
	struct person *p ;
	struct person *last;
};

struct total_records
{
	int totalPositives;
	int totalTests;
	int onVentilator;
	int inICU;
	int totalHQ;
	int totalHosp;
	int availVent;
	int availKits;
	int availBeds;
	int availICU;
};
struct person
{
	char name[30];
	int pincode;
	char gender;
	int age;
	long long int aadharNo;
	int isPositive;
	int ctValue;
	int O2Level;
	struct person *next;
};

struct total_records lastData = {0,0,0,0,0,0,0,0,0};


struct person* create_node(long long int aadhar, int age, char g, int pin, char nm[])
{
	struct person *p = (struct person*)malloc(sizeof(struct person));
	p->age = age;
	p->gender = g;
	strcpy(p->name,nm);
	p->pincode = pin;
	p->aadharNo = aadhar;
	p->isPositive = -1;
	p->ctValue = -1;
	p->O2Level = -1;
	p->next = NULL;
	return p;
}
void * register_person(void * arg)
{
	struct registrar_thread_params *reg = arg;
	FILE *f = reg->fp;
	char nm[30];
	char g;
	int age,pincode;
	long long int aadhar;
	reg->p = NULL;
	struct person *p;
	while (!feof(f))
	{
		fscanf(f, "%lld %d %c %d", &aadhar, &age, &g, &pincode);

		fgets(nm, 30, f);
		p = create_node(aadhar,age,g,pincode,nm);
		if(reg->p == NULL)
		{
			reg->p = p;
			reg->last = p;
		}
		else
		{
			reg->last->next = p;
			reg->last = reg->last->next;
			
		}
	}
	printf("\nRegisteration Completed.\n");
	pthread_exit(0);
} 

void * Testing(void * arg)
{
	struct person *phead = arg;
	int ctValue,O2=0;
	
	srand(time(0));
	
	ctValue = 5 + (rand() % 60);

	
	if(ctValue > 35)
	{
		printf("\nAdhar no -> %lld is negative.",phead->aadharNo);
		phead->isPositive = 0;
		phead->ctValue = ctValue;
	}
	else
	{
		printf("\nAdhar no -> %lld is positive.",phead->aadharNo);
		phead->isPositive = 1;
		phead->ctValue = ctValue;
		
		if(ctValue < 10)
			O2 = 60 + (rand() % 21);
		else if(ctValue > 10 && ctValue <= 25)
			O2 = 81 + (rand() % 10);
		else if(ctValue > 25 && ctValue <= 35)
			O2 = 90 + (rand() % 10);
		
		phead->O2Level = O2;
	}
	
	pthread_exit(0);
}


void * make_positive_list(void * arg)
{
	struct person *p = arg;
	FILE *fp = fopen("dataFiles/Positive_patients.txt","w");
	if(fp == NULL)
	{
		printf("\nfailed..\n");
		pthread_exit(0);
	}

	while(p->next != NULL)
	{
		if(p->isPositive == 1)
		{
			fprintf(fp,"%lld %d %d %d %s",p->aadharNo,p->ctValue,p->O2Level,p->age,p->name);
			lastData.totalPositives++;
		}
		p = p->next;
	}
	fclose(fp);
	pthread_exit(0);
}

void * make_negative_list(void * arg)
{
	struct person *p = arg;
	FILE *fp = fopen("dataFiles/negative_patients.txt","w");
	if(fp == NULL)
	{
		printf("\nfailed..\n");
		pthread_exit(0);
	}
	while(p->next != NULL)
	{
		if(p->isPositive == 0)
		{
			//fprintf(fp, "\n"); 
			fprintf(fp,"%lld %d %s",p->aadharNo,p->age,p->name);
		}
		p = p->next;	
	}
	fclose(fp);
	pthread_exit(0);
}


void * emergency (void * arg)
{
	int *o2 = arg;
	FILE *v ;
	FILE *icu;
	int ven,icub;
	v = fopen("dataFiles/Ventilators.txt","r");
	icu = fopen("dataFiles/ICU_Beds.txt","r");
	fscanf(v,"%d",&ven);
	fscanf(icu,"%d",&icub);
	fclose(v);
	fclose(icu);
	if(*o2 < 70)
	{
		if(ven == 0 && icub > 0)
		{
			printf("\n Ventilators are full. admit them into ICU.\n");
			icub--;
			icu = fopen("dataFiles/ICU_Beds.txt","w");
			fprintf(icu,"%d",icub);
			fprintf(icu,"\n");
			fclose(icu);
			lastData.inICU++;
		}	
		else if(ven > 0)
		{
			icub--;
			icu = fopen("dataFiles/ICU_Beds.txt","w");
			fprintf(icu,"%d",icub);
			fprintf(icu,"\n");
			fclose(icu);
			lastData.inICU++;
			
			ven--;
			v = fopen("dataFiles/Ventilators.txt","w");
			fprintf(v,"%d",ven);
			fprintf(v,"\n");
			fclose(v);
			lastData.onVentilator++;
			
		}
	}
	else if(*o2 >= 70 && *o2 <= 80)
	{
		icub--;
		icu = fopen("dataFiles/ICU_Beds.txt","w");
		fprintf(icu,"%d",icub);
		fclose(icu);
		lastData.inICU++;
	}
	pthread_exit(0);
}

void DisplayTodayData()
{
	FILE *kit = fopen("dataFiles/available_kits.txt","r");
	FILE *bed = fopen("dataFiles/Available_beds.txt","r");
	FILE *vent = fopen("dataFiles/Ventilators.txt","r");
	FILE *icu = fopen("dataFiles/ICU_Beds.txt","r");
	FILE *hosp = fopen("dataFiles/Hospitalize.txt","r");
	FILE *HQ = fopen("dataFiles/Home_quarantine.txt","r");
	
	fscanf(bed,"%d",&lastData.availBeds);
	fclose(bed);
	
	fscanf(kit,"%d",&lastData.availKits);
	fclose(kit);
	
	fscanf(vent,"%d",&lastData.availVent);
	fclose(vent);
	
	fscanf(icu,"%d",&lastData.availICU);
	fclose(icu);

	
	char chr = getc(hosp);
	while (chr != EOF)
    	{
		//Count whenever new line is encountered
		if (chr == '\n')
		{
		    lastData.totalHosp++;
		}
		//take next character from file.
		chr = getc(hosp);
    	}
    	fclose(hosp); 
    	
   	chr = getc(HQ);
	while (chr != EOF)
    	{
		//Count whenever new line is encountered
		if (chr == '\n')
		{
		    lastData.totalHQ++;
		}
		//take next character from file.
		chr = getc(HQ);
    	}
    	fclose(HQ); 
    	 
	printf("\n----Today's Data-------\n");
	
	//printing Records
	printf("\nTotal Tests done : %d ", lastData.totalTests);
	printf("\nTotal Positive Cases :  %d\n", lastData.totalPositives);
	printf("\nTotal Home-Quarantined Patients : %d", lastData.totalHQ);
	printf("\nTotal Available Kits at this point of time on our booth : %d\n", lastData.availKits);
	printf("\nTotal Hospitalized Patients : %d", lastData.totalHosp);
	printf("\nTotal General-Ward Beds available in Hospital as per Our test booth testing : %d\n", lastData.availBeds);
	printf("\nPatients In ICU : %d , from Which %d patients are on Ventilators.", lastData.inICU, lastData.onVentilator);
	printf("\nTotal ICU beds Available in Hospital as per Our test booth testing : %d", lastData.availICU);
	printf("\nTotal Ventilators available in Hospital as per Our test booth testing : %d", lastData.availVent);
	printf("\n\n-----That's All-----\n");	
}

void main(int argc, char *argv[])
{
	if(argc < 2)
	{
		printf("\n ERR: please provide data.\n");
		exit(1);
	}
	
	//registration phase
	pthread_t registrar;
	struct registrar_thread_params reg;
	int err=-1,ret=-1;
	
	reg.fp = fopen(argv[1], "r");
	if(reg.fp == NULL)
	{
		fprintf(stderr, "%s path or file does not exist\n", argv[1]);
		exit(1);
	}
	
	err = pthread_create (&registrar, NULL, register_person, (void *)&reg);
	if (err != 0)
	{
		printf("cant create thread: %s\n", strerror(err));	
		exit(1);
	}
	err = -1;
	sleep(1);
	
	err = pthread_join(registrar, (void **)&ret);
	if (err != 0)
	{
		printf("Joining ERROR : %s\n", strerror(err));	
		exit(1);
	}	
	 
	 
	//Testing Phase
	pthread_t TestingTeams[5];
	int cnt=0,i,*isPositive;	
	struct person *phead = reg.p;
	struct person *plast = reg.last;
	int ans[50],ac=0;
	 
	while(phead != plast)
	{
		cnt++;
		err = pthread_create (&TestingTeams[cnt%5], NULL, Testing, (void *)phead);
		if (err != 0)
		{
			printf("cant create thread: %s\n", strerror(err));	
			exit(1);
		}
		phead = phead->next;
		sleep(1);
	}
	
	printf("\n\nNumber of Patients registered : %d\n",cnt);
	lastData.totalTests = cnt;
	for(i=0;i<5;i++)
	{
		err = pthread_join(TestingTeams[i], (void **)&isPositive);
		if (err != 0)
		{
			printf("cant create thread: %s\n", strerror(err));	
			exit(1);
		}
	}
	
	//collect data and diffranciate them 
	pthread_t positives,negatives;
	phead = reg.p;
	
	err = pthread_create (&positives, NULL, make_positive_list, (void *)phead);
	if (err != 0)
	{
		printf("cant create thread: %s\n", strerror(err));	
		exit(1);
	}

	err = pthread_create (&negatives, NULL, make_negative_list, (void *)phead);
	if (err != 0)
	{
		printf("cant create thread: %s\n", strerror(err));	
		exit(1);
	}

	err = pthread_join(positives, (void **)&ret);
	if (err != 0)
	{
		printf("cant create thread: %s\n", strerror(err));	
		exit(1);
	}
	err = pthread_join(negatives, (void **)&ret);
	if (err != 0)
	{
		printf("cant create thread: %s\n", strerror(err));	
		exit(1);
	}
	
	//treatment_phase
	
	int child_pid,status;
	int child_write[2];
	pipe(child_write);
	phead = reg.p;
	child_pid = fork();
	if(child_pid == 0)
	{
		close(child_write[READ]);
		
		int kit,beds;
		char o2[4];
		
	 	FILE *fp_beds;
	 	FILE *fp_kits;
	 	FILE *fp_HQ = fopen("dataFiles/Home_quarantine.txt","w");
	 	FILE *fp_hosp = fopen("dataFiles/Hospitalize.txt","w");
		while(phead->next != NULL)
		{
			if(phead->ctValue > 25 && phead->ctValue <= 35)
			{
				fp_kits = fopen("dataFiles/available_kits.txt","r");
				fscanf(fp_kits,"%d",&kit);
				fclose(fp_kits);
				if(kit  > 0)
				{
					kit--;
					fp_kits = fopen("dataFiles/available_kits.txt","w");
					fprintf(fp_kits,"%d",kit);
					fprintf(fp_kits,"\n");
					fclose(fp_kits);
					fprintf(fp_HQ,"%lld %d %d %d %s",phead->aadharNo,phead->ctValue,phead->O2Level,phead->age,phead->name);
				}
				else
				{
					printf("\n Kits are finished.. Will provide you once it is available..\n");
				}
			}
			else if(phead->ctValue >= 10 && phead->ctValue <= 25)
			{
				fp_beds = fopen("dataFiles/Available_beds.txt","r");
				fscanf(fp_beds,"%d",&beds);
				fclose(fp_beds);
				if(beds > 0)
				{
					beds--;
					fp_beds = fopen("dataFiles/Available_beds.txt","w");
					fprintf(fp_beds,"%d",beds);
					fprintf(fp_beds,"\n");
					fclose(fp_beds);
					fprintf(fp_hosp,"%lld %d %d %d %s",phead->aadharNo,phead->ctValue,phead->O2Level,phead->age,phead->name);
				}
				else
				{
					printf("\nbeds are Full.. Trying to contact Other Hospitals..\n");
				}
			}
			else if(phead->ctValue < 10)
			{
				fprintf(fp_hosp,"%lld %d %d %d %s",phead->aadharNo,phead->ctValue,phead->O2Level,phead->age,phead->name);
				sprintf(o2,"%d",phead->O2Level);
				write(child_write[WRITE], o2, strlen(o2)+1);
			}
			phead = phead->next;	
		}	 	
		
		sprintf(o2,"%d",-1);
		write(child_write[WRITE], o2, strlen(o2)+1);
		close(child_write[WRITE]);
		fclose(fp_hosp);
		fclose(fp_HQ);
		exit(0);

	}
	else
	{
		close(child_write[WRITE]);
		int bytesRead,o2 = 99;
		char o2s[4];
		while(1)
		{
			bytesRead = read(child_write[READ],o2s,strlen(o2s)+1);
			
			o2 = atoi(o2s);
			
			if(o2 == -1)
			{
				break;
			}
			else if(o2 >= 60 && o2 <= 80)
			{
				kill( child_pid, SIGSTOP );
				
				pthread_t emergence;
				err = pthread_create (&emergence, NULL, emergency, (void *)&o2);
				if (err != 0)
				{
					printf("cant create thread: %s\n", strerror(err));	
					exit(1);
				}
				err = pthread_join(emergence, (void **)&ret);
				if (err != 0)
				{
					printf("cant create thread: %s\n", strerror(err));	
					exit(1);
				}
				kill( child_pid, SIGCONT );
			}
		}
		close(child_write[READ]);
	}
	child_pid = wait(&status);

	//Display the end of the day Records.
	
	DisplayTodayData();
}



		
/*
	int child_pid;
	int parent_write[2], child_write[2], bytesRead;
	pipe(parent_write);
	pipe(child_write);
	
	struct person *phead = reg->p;
	struct person *plast = reg->last;
	
	child_pid = fork();
	 if(chiild_pid==0)
	 {
	 	close(child_write[READ]);
	 	close(parent_write[WRITE]);
	 	
	 	int bytesRead;
	 	char aadhar[12];
	 	while(1)
	 	{
	 		bytesRead= read(parent_write[READ], response, 13);
	 		printf("testing of person having AadharNo : %lld has been started")
	 	}	
	 	
	 }	
	 else
	 {
	 	close(child_write[WRITE]);
	 	close(parent_write[READ]);
	 	
		char aadhar[12];
	 	while(*phead != NULL)
	 	{
	 		lltoa(phead->aadharNo,aadhar.DECIMAL);
	 		write(parent_write[WRITE], aadhar, strlen(aadhar)+1)
	 	}
	 }*/
