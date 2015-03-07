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

char *gen_grade(int, int, double);

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
        PGresult *enroll_count_res = PQexec(conn, enroll_count_buff);

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

        int enroll_count = PQntuples(enroll_count_res);
        int crn_count;
        int threshold = 10;
        //get crns, generate grade for each, update into table
        for (crn_count = 0; crn_count < enroll_count; crn_count++)
        {
            char *grade = (char*) malloc(sizeof(char) * 5);
            int random = rand_lim(20);
            fprintf(stderr, "rand = %d\n", random);
            int crn = atoi(PQgetvalue(enroll_count_res, crn_count, 0)); 

            //gen random grade based on gpa
            grade = gen_grade(random, threshold, gpa);
 
            if (!grade)
                exit_nicely(conn, "generating grade");

            char *update_buff = (char*) malloc(sizeof(char) * 1024);
            sprintf(update_buff, "update registry.enrollment set grade=\'%s\' where student_id = %d and crn = %d;", grade, i, crn);
            fprintf(stderr, "%s\n", update_buff);
/*
            PGresult *enroll_grade_res = PQexec(conn, update_buff);

            if (PQresultStatus(enroll_grade_res) != PGRES_TUPLES_OK)
            {
                free(update_buff);
                exit_nicely(conn, "updating grade for student");
            } 
*/
        }

        PQclear(gpa_res);

    } //for: each student, generate gpa
 
    free(student_buffer);
    PQclear(res);
    PQfinish(conn);

    return 0;
}

int rand_lim(int limit)
{
    //return a random number between 0 and limit inclusive

    int divisor = RAND_MAX/(limit+1);
    int retval;

    do {
        retval = rand() / divisor;
    } while (retval > limit);

    if (retval > 0)
        return retval -1;
    else
        return retval;
}

char *gen_grade(int random, int threshold, double gpa)
{
    char *grade = (char*) malloc(sizeof(char) * 5);

    if (gpa >= 4.0)
    {
        grade = "A";
    } 
    else if (gpa >= 3.5)
    {
        if (random > threshold + 2)
            grade = "A";
        else if (random >= threshold)
            grade = "B+";
        else
            grade = "B";
    }
    else if (gpa >= 3.0)
    {
        if (random > threshold + 5)
            grade = "A";
        else if (random >= threshold + 2)
            grade = "B+";
        else if (random >= threshold - 2)
            grade = "B";
        else
            grade = "C";
    }
    else if (gpa >= 2.5)
    {
        if (random >= threshold + 8)
            grade = "A";
        else if (random >= threshold + 7)
            grade = "B+";
        else if (random >= threshold + 5)
            grade = "B";
        else if (random >= threshold + 3)
            grade = "B-";
        else
            grade = "C";
    }
    else
    {
        if (random >= threshold + 10)
            grade = "A";
        else if (random >= threshold + 9)
            grade = "A-";
        else if (random >= threshold + 7)
            grade = "B";
        else if (random >= threshold + 5)
            grade = "B-";
        else if (random >= threshold)
            grade = "C";
        else if (random >= threshold -3)
            grade = "D";
        else
            grade = "F";
    }

    return grade;
}
