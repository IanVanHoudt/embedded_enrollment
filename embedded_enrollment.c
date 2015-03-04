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
  gcc -I /usr/include/postgresql -L /usr/lib/postgresql -o embedded_enrollment embedded_enrollment.c.c -lpq

*/

#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>


int exit_nicely(PGconn *conn, char *loc)
{
    PQfinish(conn);
    fprintf(stderr, "\n*****Whoa, had and issue (%s)! Exiting\n", loc);
    exit(1);
}


int main(int argc, char *argv[])
{

    FILE *f = fopen("debug_output.txt", "w");
    if (f == NULL)
    {
        fprintf(stderr, "Issue opening output file\n");
        exit (1);
    }

    const int DEBUG = 1;
    int input_student_id = 0;

    if (argc < 2)
        fprintf(stderr, "NOTE: You are executing with no student_id, and therefore will execute on all students\n");

    if (argc == 2)
        input_student_id = atoi(argv[1]);

    const char *conn_info;
    PGconn *conn;
    PGresult *res;

    //TODO: get correct database conn_info
    conn_info = "host=dbclass.cs.pdx.edu user=w15db71 password=secret";
    conn = PQconnectdb(conn_info);

    if (PQstatus(conn) != CONNECTION_OK)
    {
        fprintf(stderr, "Connection to DB failed: %s\n", PQerrorMessage(conn));
        return -1;
    }

    char *student_buffer = (char *) malloc (sizeof(char) * 1024);
    sprintf(student_buffer, "select s.id from registry.student s;");    
    res = PQexec(conn, student_buffer);

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        free(student_buffer);
        exit_nicely(conn, "Getting student records");
    }

    int NUM_STUD = PQntuples(res);

    //start from beginning to generate enrollment for all student
    int i = 1;     
    //if student_id provided at cli, only do so for one student
    if (input_student_id)
    {
        i = input_student_id;
        NUM_STUD = i;
    }

    if (DEBUG)
        fprintf(f, "NUM_STUD = %d\n", NUM_STUD);

    //Iterate through records, joining and finding courses and CRNs to add to enrollment
    PGresult *maj_res;    
    for (i; i <= NUM_STUD; i++)
    {
        //Find major(s) for this student (join student & student_major on student_id and student_major & major on major_id
        char *maj_buffer = (char *) malloc(sizeof(char) * 1024);
        sprintf(maj_buffer, "select maj.id from registry.student s join registry.student_major sm on s.id=sm.student_id join registry.major maj on sm.major_id=maj.id where s.id=%d;", i);
        
        maj_res = PQexec(conn, maj_buffer);
        if (PQresultStatus(maj_res) != PGRES_TUPLES_OK)
        {
            free(maj_buffer);
            exit_nicely(conn, "Getting records for student");
        }       

        int NUM_MAJ = PQntuples(maj_res);
        if (DEBUG)
            fprintf(f, "Student %d has %d majors\n", i, NUM_MAJ);

        int majors [10];
        int majoriterate;
        for (majoriterate = 0; majoriterate < NUM_MAJ; majoriterate++)
        {
            majors[majoriterate] = atoi(PQgetvalue(maj_res, majoriterate, 0));
            if (DEBUG)
                fprintf(f, "\tStudents major(s) are: %d\n", majors[majoriterate]);
        }

        int j;
        PGresult *course_of_maj_res;
        for (j = 0; j < NUM_MAJ; j++)
        {
            //access each major as majors[j] 
            char *course_buff = (char *) malloc(sizeof(char) * 1024);
            sprintf(course_buff, "select c.id from registry.major m join registry.department d on m.department_id=d.id join registry.course c on d.id=c.department_id where m.id=%d;", majors[j]);

            course_of_maj_res = PQexec(conn, course_buff);
            if (PQresultStatus(course_of_maj_res) != PGRES_TUPLES_OK)
            {
                free(course_buff);
                exit_nicely(conn, "Getting course for major");
            } 

            int NUM_COURSES = PQntuples(course_of_maj_res);
            if (DEBUG)
                fprintf(f, "\t\tMajor %d has %d courses\n", majors[j], NUM_COURSES);

            int courses [25];
            int courseiterate;
            for (courseiterate = 0; courseiterate < NUM_COURSES; courseiterate++)
            {
                courses[courseiterate] = atoi(PQgetvalue(course_of_maj_res, courseiterate, 0));
                if (DEBUG)
                    fprintf(f, "\t\t\tMajor %d includes course %d\n", majors[j], courses[courseiterate]);
            }


        } 


    }
 
    free(student_buffer);
    PQfinish(conn);
    return 0;
}
