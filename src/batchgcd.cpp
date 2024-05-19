/* --------------------------------------------
 * C++ implementation of the BatchGCD algorithm
 * --------------------------------------------
 *
 *  Feel free to use, alter, or reproduce this implementation AT YOUR OWN RISK.
 *
 *  The batch GCD algorithm, created by Daniel Bernstein [1], allows the
 *  computation of pairwise GCDs of a list of integers in quasilinear time.
 *
 *  This implementation uses the C++ wrapper around GMP arbitrary precision
 *  library, since it needs to handle potentially huge numbers. It favors
 *  writing intermediate variables to disk, so beware that, assuming that all
 *  'n' input integers are of bitLength 'l', it efficiently writes ~2n
 *  separate files, using a total disk memory of
 *
 *                               M = l * n * log(n)
 *
 *  bits. That is 10.7 GB of information if you are targeting 2 million
 *  2048-bit integers. Please do the numbers and use AT YOUR OWN RISK.
 *
 *  If you're targeting RSA keys, which is the main use-case of this algorithm
 *  and the reason that integers here are referred to as 'moduli', please
 *  consider not storing/reading the factors (the library won't write them to
 *  disk directly, but they will of course be on your RAM) and more importantly,
 *  informing compromised key owners so that they have a chance to revoke their
 *  keys / fix their implementations.
 *
 *  Please don't use this library if you're targeting a set of keys with
 *  bad intentions.
 *
 *
 *  Suggestions/comments/questions are more than welcome: fvial@protonmail.com
 */

#include <sstream>
#include <getopt.h>
#include "utils.hpp"

int N_THREADS = 1;
static int base_10_flag;

using std::cout, std::endl, std::cin, std::vector, std::ofstream;

/* Pre-requisites:
 *
 * - A file ./data/moduli.csv containing all moduli, in the format
 * <ID>,<modulus in base 16>\n
 *
 */

int main(int argc, char** argv) {
    if (argc < 2) {
        cout << "Please specify target csv file." << endl;
        exit(1);
    }
    // Detect flags
    static struct option long_options[] = {
          {"base10", no_argument, &base_10_flag, 1},
          {0, 0, 0, 0}
        };
    int option_index = 0;
    getopt_long_only(argc, argv, "", long_options, &option_index);

    // Set base
    int base = 16;
    if (base_10_flag) base = 10;

    // Prompt threads
    cout << "Define number of threads: ";
    cin >> N_THREADS;

    // Set timer
    struct timespec start, finish;
    double elapsedA, elapsedB, elapsedC;

    cout << " --------------------------------------------------- " << endl;
    cout << "| Part (A) - Product 'Z' and product tree of moduli |" << endl;
    cout << " --------------------------------------------------- " << endl;
    vector<mpz_class> input_moduli;
    vector<string> IDs;
    clock_gettime(CLOCK_MONOTONIC, &start);
    read_moduli_from_csv(argv[1], &input_moduli, &IDs, base);
    int levels = product_tree(&input_moduli);
    clock_gettime(CLOCK_MONOTONIC, &finish);
    cout << "End Part (A)" << endl;
    elapsedA = finish.tv_sec - start.tv_sec;
    elapsedA += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
    cout << "Time elapsed (s): " << elapsedA << endl << endl;

    cout << " ----------------------------------------------------- " << endl;
    cout << "| Part (B) - Compute the remainders remᵢ <- Z mod Xᵢ² |" << endl;
    cout << " ----------------------------------------------------- " << endl;
    clock_gettime(CLOCK_MONOTONIC, &start);
    vector<mpz_class> R;
    remainders_squares(levels, &R);
    cout << "End Part (B)" << endl;
    clock_gettime(CLOCK_MONOTONIC, &finish);
    elapsedB = (finish.tv_sec - start.tv_sec);
    elapsedB += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
    cout << "Time elapsed (s): " << elapsedB << endl << endl;

    cout << " ------------------------------------------------------  " << endl;
    cout << "|Part (C) - Compute final GCDs (remᵢ <- remᵢ/Xᵢ mod Xᵢ) |" << endl;
    cout << " ------------------------------------------------------  " << endl;
    clock_gettime(CLOCK_MONOTONIC, &start);
    cout << "Re-reading moduli (were destroyed in part B)" << endl;
    read_level_from_file(0, &input_moduli);
    for (unsigned int i = 0; i < input_moduli.size(); i++) {
        R[i] = R[i] / input_moduli[i];
        R[i] = gcd(R[i], input_moduli[i]);
    }
    clock_gettime(CLOCK_MONOTONIC, &finish);
    elapsedC = (finish.tv_sec - start.tv_sec);
    elapsedC += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;


    cout << endl;
    cout << "   *****************************  " << endl;
    cout << "   *****************************  " << endl;
    cout << "   *  Total time elapsed (s):  *" << endl;
    int totalSec = static_cast<int>(elapsedA + elapsedB + elapsedC);
    int totalMin = totalSec / 60;
    int totalHour = totalMin / 60;
    totalSec %= 60;
    totalMin %= 60;
    cout << "      ";
    cout << totalHour << "h " << totalMin << "m " << totalSec << "s " << endl;
    cout << "   *****************************  " << endl;
    cout << "   *****************************  " << endl << endl;

    cout << "Verifying correctness before announcing results" << endl << endl;
    vector<string> compromised;
    vector<string> duplicates;
    int false_positives = 0;
    // False positives should not exist, this is a sanity check for large input
    // sets.
    for (int i = 0; i < static_cast<int>(input_moduli.size()); i++) {
        if (R[i] != 1) {
            if (R[i] == 0 || input_moduli[i] % R[i] != 0) {
                false_positives += 1;
            } else if (R[i] == input_moduli[i]) {
                duplicates.push_back(IDs[i]);
            } else {
                mpz_class factor_p = R[i];
                mpz_class factor_q = input_moduli[i] / R[i];
                std::ostringstream os;
                os << IDs[i] <<"," << factor_p.get_str() << "," << factor_q.get_str();
                compromised.push_back(os.str());
            }
        }
    }
    cout << "    ------------- " << endl;
    cout << "   |-- Results --|" << endl;
    cout << "    ------------- " << endl << endl;
    cout << "Amount of target moduli:       " << input_moduli.size() << endl;
    cout << "Amount of duplicates:          " << duplicates.size() << endl;
    cout << "Amount of compromised moduli:  " << compromised.size() << endl;
    cout << "False positives:               " << false_positives << endl;
    cout << "Writing compromised IDs to file..." << endl;
    string line = "";
    ofstream file;
    file.open("compromised.csv");
    for (unsigned int i = 0; i < compromised.size(); i++) {
        file << compromised[i] << "\n";
    }
    file.close();
    file.open("duplicates.csv");
    for (unsigned int i = 0; i < duplicates.size(); i++) {
        file << duplicates[i] << "\n";
    }
    file.close();
    if (duplicates.size()) {
        cout << "Note: filter duplicates directly from the input file ";
        cout << "(i.e., ignoring the output file)" << endl;
        cout << "and run again. They may contain compromised moduli. ";
        cout << "If you already did this, then all\nintegers marked as ";
        cout << "duplicate share factors (run naïve GCDs).";
        cout << endl << endl;
    }
    cout << endl << "See results in compromised.csv and duplicates.csv" << endl;
    cout << "Done, bye." << endl;
    return 0;
}

// Only for fair benchmarking against factorable.net
void output_base16(vector<mpz_class> *X) {
    ofstream file;
    file.open("base16.moduli");
    for (unsigned int i = 0; i < X->size(); i++) {
        file << X->at(i).get_str(16) << "\n";
    }
    file.close();
}

