


#include <fstream>
#include <iostream>
#include <cstdlib>
#include <stdlib.h>
#include <string.h>
#include <ilcplex/ilocplex.h>
#include "Functions.h"
#include "data.h"
#include "cpuTime.h"

using namespace std;

int main(int argc, char *argv[])
{


	Data data(argv);
	preprocess(data);

	cout << "Exact solving method" << endl;

	cout << "instance info:" << endl;

	cout << "instance: " << data.instance << endl;
	cout << "machines: " << data.machines << endl;
	cout << "jobs: " << data.jobs << endl;
	cout << "slits: " << data.slits << endl;
	cout << "colors: " << data.colors << endl;
	cout << "machine_width: " << data.mWidth << endl;
	cout << endl;

	for (int i = 0; i < data.jobs; i++)
	{
		cout << data.jweight[i] << " ";
		cout << data.jwidth[i] << " ";
		cout << data.jlength[i] << " ";
		cout << data.jthick[i] << " ";
		cout << data.jcolor[i] << endl;
	}


	for (int i = 0; i < data.jobs; i ++){
		for (int j = 0; j < data.jobs; j++){
			cout << data.setup[i][j] << " ";
		}
		cout << endl;
	}


	char var[100];

	IloEnv env;
	IloModel model(env, "SPP");
	IloCplex cplex(model);


	//creating variables

	//X_ijk
	IloArray <IloArray <IloBoolVarArray> > x(env, data.jobs+1);


	for (int i = 0; i < data.jobs+1; i++){
		IloArray <IloBoolVarArray> plano(env, data.jobs+1);
		x[i] = plano;
	}

	for (int i = 0; i < data.jobs+1; i++){
		for (int j = 0;	j < data.jobs+1; j++){
			IloBoolVarArray vetor (env, data.jobs+1);
			x[i][j] = vetor;
		}
	}

	//A_k and B_k

	IloNumVarArray B(env, data.jobs+1, 0, 999999);
	IloNumVarArray A(env, data.jobs+1, 0, 999999);


	//S_hk

	IloArray <IloBoolVarArray> S(env, data.jobs+2);

	for (int i = 0; i <= data.jobs+1; i++){
		S[i] = IloBoolVarArray(env, data.jobs+2);
	}

	//f
	IloArray<IloNumVarArray> F(env, data.jobs+2);
	// NumVarMatrix F(env, data.jobs+2);
	for(int i = 0; i < data.jobs+2; i++){
		F[i] = IloNumVarArray(env, data.jobs+2, 0.0, 10000000);
	}

	//z
	IloNumVar z(env, 0, 9999999, ILOINT);


	//adding them to the model

	//X_ijk
	for (int i = 1;	i < data.jobs+1; i++){
		for (int j = 1; j <= i; j++){
			for (int k = 1; k <= j; k++){
				if (data.jcolor[i-1] == data.jcolor[j-1] && data.jcolor[j-1] == data.jcolor[k-1] && data.jthick[i-1] == data.jthick[j-1] && data.jthick[j-1] == data.jthick[k-1]){
					// cout << i << ", " << j << ", " << k << endl;
					sprintf(var, "x_%d_%d_%d", i, j, k);
					x[i][j][k].setName(var);
					model.add( x[i][j][k] );
				}
			}
		}
	}


	//A_k and B_k
	for (int k = 1; k < data.jobs+1; k++){

		sprintf(var, "B_%d", k);
		B[k].setName(var);
		model.add( B[k] );

		sprintf(var, "A_%d", k);
		A[k].setName(var);
		model.add( A[k] );

	}

	//S_hk
	for (int h = 0; h < data.jobs+2; h++){
		for (int k = 1; k < data.jobs+2; k++){
			if (k != h){
				sprintf(var, "S_%d_%d", h, k);
				S[h][k].setName(var);
				model.add( S[h][k] );
			}
		}
	}

	//f
	for(int i = 0; i < data.jobs+1; i++){
		for (int j = 1; j < data.jobs+2; j++){
			if (i != j && (i != 5 || j != 0)){
				sprintf (var, "F_%d_%d", i, j);
				F[i][j].setName(var);
				model.add( F[i][j] );
			}
		}
	}

	//z
	sprintf(var, "Z");
	z.setName(var);
	model.add( z );


	//-----------Obj Function------------\/

	IloExpr objFunction(env);

	objFunction += z;
		 	
	model.add(IloMinimize(env, objFunction));





	// cout << "constraints " << endl;

	//------------->constraints----------\/


	//every job has to be assigned once
	for (int i = 1;	i < data.jobs+1; i++){
		IloExpr exp (env);
		sprintf (var, "Constraint1_%d", i);

		for (int j = 1; j <= i; j++){
			for (int k = 1; k <= j; k++){
				if (data.jcolor[i-1] == data.jcolor[j-1] && data.jcolor[j-1] == data.jcolor[k-1] && data.jthick[i-1] == data.jthick[j-1] && data.jthick[j-1] == data.jthick[k-1]){
					exp += x[i][j][k];
				}
			}
		}

		IloRange cons = (exp == 1);
		cons.setName(var);
		model.add(cons);
	}


	//the sum of the width's on every used block has to be <= than the strip width
	for (int k = 1; k < data.jobs+1; k++){
		IloExpr exp (env);
		sprintf (var, "Constraint2_%d", k);

		for (int i = k;	i < data.jobs+1; i++){
			// cout << k << ", " << i << endl;
			if (data.jcolor[i-1] == data.jcolor[k-1] && data.jthick[i-1] == data.jthick[k-1]){
				exp += x[i][i][k] * data.jwidth[i-1];
			}
		}

		exp -= (data.mWidth * x[k][k][k]);

		IloRange cons = (exp <= 0);
		cons.setName(var);
		model.add(cons);
	}

	//forbid job i to go to a lower shelve withou he shelve being used
	for (int i = 1;	i < data.jobs+1; i++){
		for (int j = 1; j <= i; j++){
			for (int k = 1; k <= j; k++){
				if (i != j && data.jcolor[i-1] == data.jcolor[j-1] && data.jcolor[j-1] == data.jcolor[k-1] && data.jthick[i-1] == data.jthick[j-1] && data.jthick[j-1] == data.jthick[k-1]){
					IloExpr exp (env);
					sprintf (var, "Constraint3_%d_%d_%d", i, j, k);

					exp += (x[i][j][k] - x[j][j][k]);

					IloRange cons = (exp <= 0);
					cons.setName(var);
					model.add(cons);
				}
			}
		}
	}



	//maximum number of slits
	for (int k = 1; k < data.jobs+1; k++){
		IloExpr exp (env);
		sprintf (var, "Constraint4_%d", k);

		for (int i = k;	i < data.jobs+1; i++){
			if (data.jcolor[i-1] == data.jcolor[k-1] && data.jthick[i-1] == data.jthick[k-1]){

				exp += x[i][i][k];
			}
		}

		IloRange cons = (exp <= data.slits);
		cons.setName(var);
		model.add(cons);
	}


	//B_k calculation
	for (int k = 1; k < data.jobs+1; k++){
		for (int j = 1; j < data.jobs+1; j++){

			IloExpr exp (env);
			sprintf (var, "Constraint5_%d_%d", j, k);

			exp += B[k];

			for (int i = 1; i < data.jobs+1; i++){
				if (i >= j && j >= k && data.jcolor[i-1] == data.jcolor[j-1] && data.jcolor[j-1] == data.jcolor[k-1] && data.jthick[i-1] == data.jthick[j-1] && data.jthick[j-1] == data.jthick[k-1]){
					exp -= x[i][j][k] * data.jlength[i-1];
				}
			}

			IloRange cons = (exp >= 0);
			cons.setName(var);
			model.add(cons);
		}
	}



	IloExpr teste (env);
	for (int i = 1; i < data.jobs+1; i++){
		teste += S[0][i];
	}

	IloRange consTeste = (teste == data.machines);
	model.add(consTeste);





	for (int k = 1; k < data.jobs+2; k++){

		IloExpr exp (env);
		sprintf (var, "Constraint6_%d", k);


		// if (k > 0){
			for (int h = 0; h < data.jobs+1; h++){
				if (h != k){
					exp -= S[h][k];
				}
			}
		// }

		// if ( k <= data.jobs){
			for (int h = 1; h < data.jobs+2; h++){
				if (h != k){
					exp += S[k][h];
				}
			}
		// }

		if (k == 0){
			IloRange cons = (exp == data.machines);
			// IloRange cons = (exp == 0);
			cons.setName(var);
			model.add(cons);
		} else if (k >= 1 && k <= data.jobs){
			// exp -= x[k][k][k];
			IloRange cons = (exp == 0);
			cons.setName(var);
			model.add(cons);
		} else {
			// for (int i = 1; i < data.jobs+1; i ++){
			// 	exp -= x[i][i][i];
			// }
			IloRange cons = (exp == -data.machines);
			cons.setName(var);
			model.add(cons);
		}
	}


	// cout << "!" << endl;
	//a block can be used in the route only if it has been created
	for (int k = 1; k < data.jobs+1; k++){
		IloExpr exp (env);
		sprintf (var, "Constraint7_%d", k);

		for (int h = 0; h < data.jobs+1; h++){
			if (h != k){
				exp += S[h][k];
			}
		}

		exp -= x[k][k][k];

		IloRange cons = (exp == 0);
		cons.setName(var);
		model.add(cons);
	}

	// cout << "1" << endl;

	//A_k calculation
	for (int k = 1; k < data.jobs+1; k++){

		IloExpr exp (env);
		sprintf (var, "Constraint8_%d", k);

		for (int h = 1; h < data.jobs+1; h++){
			if (k != h){
				// if (h == 0){
					// exp += 0;
				// } else {
					exp += data.setup[h-1][k-1] * S[h][k];
				// }
			}
		}

		exp -= A[k];

		IloRange cons = (exp <= 0);
		cons.setName(var);
		model.add(cons);
	}

	// cout << "2" << endl;


	for (int k = 0; k < data.jobs+1; k++){

		IloExpr exp (env);
		sprintf (var, "Constraint9_%d", k);

		if ( k < data.jobs+1){
			for (int i = 1; i < data.jobs+2; i++){
				if (k != i){
					exp += F[k][i];
				}
			}
		}

		if (k > 0){
			for (int j = 1; j < data.jobs+1; j++){
				if (k != j){
					exp -= F[j][k];
				}
			}
		}

		if (k > 0 && k < data.jobs+1){
			exp -= (A[k] + B[k]);
		}
		if (k == data.jobs+1){
			exp += z;
		}
		IloRange cons = (exp == 0);
		cons.setName(var);
		model.add(cons);

	}


	// //NEW flow constraint
	// IloExpr exp2 (env);
	// sprintf (var, "Constraint9.1");

	// for (int i = 1; i < data.jobs+2; i++){
	// 	exp2 += F[0][i];
	// }

	// IloRange cons1 = (exp2 == 0);
	// cons1.setName(var);
	// model.add(cons1);

	// // cout << "3" << endl;

	// for (int k = 1; k < data.jobs+1; k++){

	// 	IloExpr exp (env);
	// 	sprintf (var, "Constraint9.2_%d", k);

	// 	// if ( k < data.jobs+1){
	// 		for (int i = 1; i < data.jobs+2; i++){
	// 			if (k != i){
	// 				exp += F[k][i];
	// 			}
	// 		}
	// 	// }

	// 	for (int j = 1; j < data.jobs+1; j++){
	// 		if (k != j){
	// 			exp -= F[j][k];
	// 		}
	// 	}

	// 	exp -= (A[k] + B[k]);
	// 	IloRange cons = (exp == 0);
	// 	cons.setName(var);
	// 	model.add(cons);

	// }

	// // cout << "4" << endl;

	for (int i = 0; i < data.jobs+1; i++){

		IloExpr exp(env);
		sprintf (var, "Constraint9.3_%d", i);

		exp -= F[i][data.jobs+1];

		exp += z;

		IloRange cons = (exp >= 0);
		cons.setName(var);
		model.add(cons);

	}



	//compute T
	int T = 0;
	for (int i = 0; i < data.jobs; i++){
		T += data.jlength[i];
	}

	// cout << "T: " << T << endl;
	// getchar();




	for (int h = 0; h < data.jobs+1; h++){
		for (int k = 1; k < data.jobs+2; k++){
			if (h != k){
				IloExpr exp(env);
				sprintf (var, "Constraint10_%d_%d", h, k);

				exp += F[h][k];
				exp -= T*S[h][k];

				IloRange cons = (exp <= 0);
				cons.setName(var);
				model.add(cons);
			}
		}
	}


	cplex.exportModel("SPP.lp");




	cplex.setParam(IloCplex::Threads, 1);
	cplex.setParam(IloCplex::TreLim, 10000);
	cplex.setParam(IloCplex::TiLim, 3600);
	cplex.setParam(IloCplex::EpGap, 0.0000000001);
	cplex.setParam(IloCplex::EpInt, 0.0000000001);
	// cplex.setParam(IloCplex::EpOpt, 0.2);
	// if (data->cbk == true){
	// 	cplex.setParam(IloCplex::NodeLim, 1);
	// }

	double time = cpuTime();
	bool feasible = cplex.solve();
	time = cpuTime() - time;

	data.status = cplex.getCplexStatus();


	if (cplex.getSolnPoolNsolns() > 0){
		data.UB = floor(cplex.getObjValue() + 0.5);
	} else{
		data.UB = 100000;
	}

	data.LB = cplex.getBestObjValue();
	data.nNodes = cplex.getNnodes();

	cout << "status: " << data.status << endl;
	cout << "UB: " << data.UB << endl;

	ofstream out ("ExactOutPutSet5_4.txt", ios::app);

	out << data.instance << "\t" << data.UB << "\t" << data.LB << "\t" << 0  << "\t" << data.nNodes << "\t" << time << "\t" << data.status << endl;






	









	// cplex.setParam(IloCplex::Threads, 1);
	// // cplex.exportModel("SPP.lp");

	// // return 0;
	

	// cplex.solve();

	// cout << "Objectine Function: " << cplex.getObjValue() << endl;

	// // cout << "Solved" << endl;


	// cout << "Created bloks:" << endl;
	// for (int k = 1; k < data.jobs+1; k++){

	// 	if(cplex.getValue(x[k][k][k] >= 1 - 0.0000001)){

	// 		cout << k << ": " << endl;

	// 		for (int j = k; j < data.jobs+1; j++){

	// 			if (data.jcolor[j-1] == data.jcolor[k-1] && data.jthick[j-1] == data.jthick[k-1] && cplex.getValue(x[j][j][k] >= 1 - 0.00000001)){

	// 				cout << "\t";

	// 				for (int i = j; i < data.jobs+1; i++){

	// 					if (data.jcolor[i-1] == data.jcolor[j-1] && data.jcolor[j-1] == data.jcolor[k-1] && data.jthick[i-1] == data.jthick[j-1] && data.jthick[j-1] == data.jthick[k-1] && cplex.getValue(x[i][j][k]) >= 1 - 0.00000001){
	// 						cout << i << " ";
	// 					}
	// 				}
	// 				cout << endl;
	// 			}
	// 		}
	// 		cout << endl;
	// 	}
	// }


	// for (int i = 0; i < data.jobs+2; i++)
	// {
	// 	for (int j = 1; j < data.jobs+2; j++)
	// 	{
	// 		if ( i != j && cplex.getValue(S[i][j]) >= 1 - 0.00000001)
	// 		{
	// 			cout << "S_" << i << "," << j << endl;
	// 		}
	// 	}
	// }

	// for (int i = 0; i < data.jobs+1; i++)
	// {
	// 	for (int j = 1; j < data.jobs+2; j++)
	// 	{
	// 		if ( i != j )
	// 		{
	// 			if ( cplex.getValue(F[i][j]) > 0.1 && cplex.getValue(F[i][j]) < 10000000 ){
	// 				cout << "F_" << i << "," << j << ": " << setprecision(15) << cplex.getValue(F[i][j]) << endl;
	// 			}
	// 		}
	// 	}
	// }


	// for (int i = 1; i < data.jobs+1; i++)
	// {
	// 	cout << "A_" << i << ": " << cplex.getValue(A[i]) << endl;
	// }

	// for (int i = 1; i < data.jobs+1; i++)
	// {
	// 	cout << "B_" << i << ": " << cplex.getValue(B[i]) << endl;
	// }

	//getchar();
	return 0;

}
