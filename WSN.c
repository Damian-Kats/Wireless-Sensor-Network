#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>

#define xmax 4
#define ymax 15

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("This program requires a command line argument to declare the threshold\n");
		return 1;
	}
	int rank, value, size, start, token = 1, r, i, j, m; //Initialise variables
	int posx;
	int posy;
	char *ptr;
	int threshold = strtol(argv[1], &ptr, 10);
	int array[4];

	FILE *fp;
	fp = freopen("output.txt", "a+", stdout);

	double grid[4][15]; //Initialise grid of 60 nodes, 4x15
	
	start = MPI_Init(&argc, &argv); //Initialise MPI execution environment

	if (start != MPI_SUCCESS) //If MPI fails then print error and abort
	{
		printf("Error starting MPI program, terminating.\n");
		MPI_Abort(MPI_COMM_WORLD, start);
	}

	MPI_Comm_rank(MPI_COMM_WORLD, &rank); //Determine rank of calling process

	MPI_Comm_size(MPI_COMM_WORLD, &size); //Determine number of processes

	clock_t begin = clock(); //Calculate global start time
	float begin_sec = (double)(begin) / CLOCKS_PER_SEC; //Convert to seconds

	float global_begin;
	MPI_Reduce(&begin_sec, &global_begin, 1, MPI_FLOAT, MPI_MIN, 60, MPI_COMM_WORLD);
	

	m = 0;
	for (i = 0; i < xmax; i++) //Fill the grid with the ranks of each process in order
	{
		for (j = 0; j < ymax; j++)
		{
			grid[i][j] = m;
			//printf("grid[%d][%d] = %f\n", i, j, grid[i][j]);
			if (m == rank)
			{
				posx = j;
				posy = i;
			}
			m++;
		}
	}	

	srand(time(NULL) + rank); //set seed for random number generator, +rank is to make seed different for every process
	r = rand() % 50; //returns pseudo-random integer between 0 and 50

	if (rank < 60) //Instructions for WSN nodes not including base station
	{
		//printf("Rank: %d, posy = %d, posx = %d, r = %d\n", rank, posy, posx, r);
	}

	int up = posy - 1; //Variable for adjacent node in row above
	int down = posy + 1; //Variable for adjacent node in row below
	int left = posx - 1; //Variable for adjacent node to the left
	int right = posx + 1; //Variable for adjacent node to the right

	/*Instructions for nodes above threshold*/
	if (r >= threshold && rank < 60) 
	{
		if (up >= 0)//Send message to processor in row above
		{
			MPI_Send(&r, 1, MPI_INT, grid[up][posx], 0, MPI_COMM_WORLD);
			//printf("Node %d sent %d to node %f\n", rank, r, grid[up][posx]);
		}

		if (down <= 3)//Send message to processor in row below
		{
			MPI_Send(&r, 1, MPI_INT, grid[down][posx], 0, MPI_COMM_WORLD);
			//printf("Node %d sent %d to node %f\n", rank, r, grid[down][posx]);
		}

		if (left >= 0)//Send message to processor adjacent to left
		{
			MPI_Send(&r, 1, MPI_INT, grid[posy][left], 0, MPI_COMM_WORLD);
			//printf("Node %d sent %d to node %f\n", rank, r, grid[posy][left]);
		}

		if (right <= 14)//Send message to processor adjacent to right
		{
			MPI_Send(&r, 1, MPI_INT, grid[posy][right], 0, MPI_COMM_WORLD);
			//printf("Node %d sent %d to node %f\n", rank, r, grid[posy][right]);
		}
		MPI_Barrier(MPI_COMM_WORLD);
	}

	/*Instructions for nodes below threshold*/
	else if (rank < 60) 
	{
		int flag = -1; //Initialise variables
		int count = 0;
		MPI_Status status;
		MPI_Request request;

		for (int i = 0; i < 4; i++) //Iterate max four times because a node can receive a max of four messages from adjacent nodes
		{
			if (flag != 0) //Attempt to receive messages from adjacent nodes
			{
				MPI_Irecv(&r, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &request);
				flag = 0;
			}
			
			MPI_Test(&request, &flag, &status); //Return flag = true if request succeeded

			if (flag != 0) //if flag = true then node received a message
			{
				//printf("Current node: %d, recv: %d, node: %d\n", rank, r, status.MPI_SOURCE);
				array[count] = r;
				count++; //Increment counter
			}

			flag = -1;

			if (count == 4) //If four messages received, notify base station of event
			{
				MPI_Send(&array, 4, MPI_INT, 60, 0, MPI_COMM_WORLD);
				//printf("Node %d sent %d to base station\n", rank, r);
				break;
			}
			
			
		}
		//printf("\n");
		MPI_Barrier(MPI_COMM_WORLD);
	}
	else //Instructions for base station at node 61
	{
		int done = 0; //Initialise variables
		int flag = -1;
		int events = 0;
		MPI_Status status;
		MPI_Request request;

		MPI_Barrier(MPI_COMM_WORLD);
		//printf("Base station, node: 61\n"); 

		while (!done) //While potential messages still exist to receive
		{
			if (flag != 0) //Attempt to receive messages from reference nodes
			{
				MPI_Irecv(&array, 4, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &request);
				flag = 0;
			}
			
			MPI_Test(&request, &flag, &status); //Return flag = true if request succeeded

			if (flag != 0) //if flag = true then base station received a message and therefore event occurred at reference node
			{
				printf("Event detected at reference node %d\n", status.MPI_SOURCE);
				printf("Adjacent nodes: node %d with r = %d, node %d with r = %d, node %d with r = %d, node %d with r = %d\n\n", status.MPI_SOURCE - 15, array[0], status.MPI_SOURCE - 1, array[1],  status.MPI_SOURCE + 1, array[2], status.MPI_SOURCE + 15, array[3]);
				flag = -1;
				events++;
				//done = 1;
			}
			else if (flag == 0 && events > 0) //If no more messages and events have occurred
			{				
				printf("Number of events detected: %d\n\n", events);
				printf("Threshold: %d\n\n", threshold);
				break;
			}
			else //If no more messages and events haven't occurred
			{
				printf("No events detected\n");
				done = 1;
			}
			
		}
	}
	clock_t end = clock(); //Calculate end time for this process
	float end_sec = (double)(end) / CLOCKS_PER_SEC;

	float global_end; //Reduce end times of all processes to the min time		
	MPI_Reduce(&end_sec, &global_end, 1, MPI_FLOAT, MPI_MAX, 60, MPI_COMM_WORLD);
	
	if (rank == 60) //Print global simulation time by minusing global start time from global end time
	{
		printf("Simulation time = %f\n\n", global_end - global_begin);
	}
	
	MPI_Finalize();
	fclose(fp);

	return 0;
}
