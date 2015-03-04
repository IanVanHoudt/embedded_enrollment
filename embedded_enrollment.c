/*
  Ian Van Houdt
  CS 586
  embedded_enrollment.c

  This file will use an embedded SQL routine to generate enrollment records in the student
  registry database. It will go through student records, find each student's major(s), join
  the requisite tables to find the course_ids for each major, check that prerequisites are
  already taken, then either adds that course or continues to randomly select courses from 
  that major and try to add them.

  There is the potential that an unlucky random selection of course will leave a student with
  no enrollment (each randomly selected course required a course that wasn't already taken). 
  This is easily fixed by re-running the program with that student's id as an argument.  Worst
  case scenario is that you have to run this a few time and prune the results (if a student has
  taken 5 classes every term or something like that).

  Compile as: 
  gcc -I /usr/include/postgresql -L /usr/lib/postgresql -o embedded_raise embedded_raise.c -lpq

*/

#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>


int exit_nicely(PGconn *conn, char *loc = "")
{
    PQfinish(conn);
    fprintf(stderr, "\n*****Whoa, had and issue (in: %s)! Exiting\n", loc);
    exit(1);
}


int main(int argc, char *argv[])
{
    //char *STUDENT_ID = "student.id";
    //char *MAJOR_ID = "major.id";

    const int DEBUG = 1;
    int input_student_id 0;

    if (argc < 2)
        fprintf(stderr, "NOTE: You are executing with no student_id, and therefore will execute on all students\n");

    if (argc == 2)
        input_student_id = atoi(argv[1]);

    const char *conn_info;
    PGconn *conn;
    PGresult *res;

    //TODO: get correct database conn_info
    conn_info = "host=dbclass.cs.pdx.edu user=w15db9 password=secret";
    conn = PQconnectdb(conn_info);

    if (PQstatus(conn) != CONNECTION_OK)
    {
        fprintf(stderr, "Connection to DB failed: %s\n", PQerrorMessage(conn));
        return -1;
    }

    char *student_buffer = (char *) malloc (sizeof(char) * 1024);
    sprintf(buffer, "select s.id from student s");    
    res = PQexec(conn, buffer);

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        free(student_buffer);
        exit_nicely(conn, "Getting student records");

    int NUM_OF_STUDENTS = PQntuples(res);

    //start from beginning to generate enrollment for all student
    int i = 1;     
    //if student_id provided at cli, only do so for one student
    if (input_student_id)
    {
        i = input_student_id;
        NUM_OF_STUDENTS = i;
    }

    if (DEBUG)
        fprintf(stderr, "NUM_OF_STUDENTS = %d\n", NUM_OF_STUDENTS);

    //Iterate through records, joining and finding courses and CRNs to add to enrollment
    PGresult *maj_res;    
    for (i; i <= NUM_OF_STUDENTS; i++)
    {
        //Find major(s) for this student (join student & student_major on student_id and student_major & major on major_id
        char *maj_buffer = (char *) malloc(sizeof(char) * 1024);
        sprintf(maj_buffer, "select maj.id from student s join student_major sm on s.id=sm.student_id join major maj on \ 
                             sm.major_id=maj.id having s.id=%d", i);
        
        maj_res = PQexec(conn, maj_buffer);
        //
    }
 
    free(student_buffer);
    PQfinish(conn);
    return 0;
}
