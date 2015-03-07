/*
  Ian Van Houdt
  CS 586
  embedded_enrollment_grades.c

  This file will use an embedded SQL routine to generate grades in the Enrollment table of the student
  registry database. Grades will be randomly selected within a range dictated by their existing gpa
  entry in the Student table.


  Compile as: 
  gcc -I /usr/include/postgresql -L /usr/lib/postgresql -o embedded_enrollment embedded_enrollment.c.c -lpq

*/

#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include <string.h>

int exit_nicely(PGconn *conn, char *loc)
{
    PQfinish(conn);
    fprintf(stderr, "\n*****Whoa, had and issue (%s)! Exiting\n", loc);
    exit(1);
}


int main(int argc, char *argv[])
{
    const int DEBUG = 1;
    FILE *f = fopen("debug_output.txt", "w");
    if (f == NULL)
    {
        fprintf(stderr, "Issue opening output file\n");
        exit (1);
    }

    const char *conn_info;
    PGconn *conn;
    PGresult *res;

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
    int i;
    //Iterate through records FOR EACH STUDENT, joining and finding courses and CRNs to add to enrollment
    //Student_ids start at 1 (i = 1)
    for (i = 1; i <= NUM_STUD; i++)
    {
        //First, snag the students enrollment date
        char *gpa_buff = (char *) malloc(sizeof(char) * 1024);
        sprintf(gpa_buff, "select s.gpa from registry.student s where s.id=%d;", i);
        PGresult *gpa_res = PQexec(conn, gpa_buff);

        if (PQntuples(gpa_res) < 1)
        {
            PQclear(gpa_res);
            continue;
        }

        if (PQresultStatus(gpa_res) != PGRES_TUPLES_OK)
        {
            free(gpa_buff);
            exit_nicely(conn, "Getting gpa for student");
        } 

        double gpa = atof(PQgetvalue(gpa_res, 0, 0));

        //get number of courses for student
        char *enroll_count_buff = (char*) malloc(sizeof(char) * 1024);
        sprintf(enroll_count_buff, "select crn from registry.enrollment where student_id = %d", i);
        RGresult *enroll_count_res = PQexec(conn, enroll_count_buff);

        if (PQntuples(enroll_count_res) < 1)
        {
            PQclear(enroll_count_res);
            continue;
        }
 
        if (PQresultStatus(enroll_count_res) != PGRES_TUPLES_OK)
        {
            free(enroll_count_buff);
            exit_nicely(conn, "Getting enrollment count for student");
        } 

        int enroll_count = PQntuples(enroll_count_buff);
        int crn[30];
        int crn_count;

        for (crn_count = 0; crn_count < enroll_count; crn_count++)
        {
            crn[crn_count] = atoi(PQgetvalue(enroll_count_res, crn_count, 0));
 
        }

        PQclear(gpa_res);

    } //for: each student, generate gpa
 
    free(student_buffer);
    PQclear(res);
    PQfinish(conn);

    return 0;
}


