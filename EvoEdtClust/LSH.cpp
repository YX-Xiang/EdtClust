#include <omp.h>
#include "LSH.h"
#include "Utils.h"


DSU::DSU(int n) {
    fa = std::vector <int> (n);
    rank = std::vector <int> (n);
    for(int i = 0; i < n; i ++) {
        fa[i] = i, rank[i] = 1;
    }
}


DSU::~DSU() { }


int DSU::find(int x) {
    if(x == fa[x]) return x;
    return fa[x] = find(fa[x]);
}


void DSU::merge(int x, int y) {
    int faX = find(x), faY = find(y);
    if(faX == faY) return;
    if(rank[faX] < rank[faY]) {
        fa[faX] = faY;
    } else if(rank[faX] > rank[faY]) {
        fa[faY] = faX;
    } else {
        fa[faX] = faY;
        rank[faY] ++;
    }
}


Fingerprint::Fingerprint(int Q = 0) {
    key = std::vector <int> (Q);
}


Fingerprint::~Fingerprint() { }


bool Fingerprint::operator==(const Fingerprint& o) const {
    return key == o.key;
}


// Hash for a vector, defined for unordered_map <Fingerprint, int> bucket;
struct FingerprintHasher {
    std::size_t operator() (const Fingerprint& x) const {
        std::size_t hash = x.key.size();
        for(auto i: x.key) {
            hash ^= i + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};


PStableLSH::PStableLSH(const ClusterNode& node, const GappedKmerEmbedding& gke, double sim):
    node(node), gke(gke), dsu(node.n), sim(sim), Qlist({10, 4}), Tlist({50, 5}) {
        std::cout << "=== Partitioning this set use p-stable LSH " << std::endl;
        Q = Qlist[node.level];
        T = Tlist[node.level];
        std::cout << "+++ use " << Q << " hash values as a key, repeat " << T << " times" << std::endl;

auto st0 = Timer::set_start();
        similarityPairEstimation = sample();
Timer::time_profile("-- LSH init -- sample estimation: ", st0);
auto st1 = Timer::set_start();
        sigma = getOptimalSigma();
Timer::time_profile("-- LSH init -- optSigma: ", st1);
        std::cout << "+++ Due to " << node.n << " sequences to cluster, optimal sigma is " << sigma << std::endl;
}


PStableLSH::~PStableLSH() { }


int PStableLSH::rdSeed = 1;


int PStableLSH::nextRdSeed() {
    rdSeed = rdSeed * 1664525 + 1013904223;
    return rdSeed;
}


double PStableLSH::__f(double x) {
    return 2.0 / std::acos(-1.0) * std::atan(x) - (1.0 / (std::acos(-1.0) * x)) * std::log(1.0 + x * x);
}


// hard part ***
double PStableLSH::getOptimalSigma() {
    if(similarityPairEstimation == 1) {
        similarityPairEstimation = 1 - 1e-4;
    }
    targetFP = 1.0 / (double)(node.n * (node.n - 1) * (1 - similarityPairEstimation) / 2.0);
    targetP2 = std::pow(targetFP, 1.0/Q);
    //std::cerr << "~~~ oldp2 = " << oldP2 << ", targetp2 = " << targetP2 << std::endl;
    double l = 1e-6, r = 100.0, mid;
    while(l - r < - 1e-4) {
        mid = (r + l) / 2.0;
        if(__f(mid) - targetP2 < - 1e-4) {
            l = mid + 1e-6;
        } else {
            r = mid - 1e-6;
        }
    }
    //return mid * 2.0 * gke.calcNGKmer(median_l); 
    return mid * 2.0 * gke.avgNGKmer;
}


double PStableLSH::genCauchy() {
    std::uniform_real_distribution<double> uniDist(0.0, 1.0);
    //std::random_device rd;
    //std::mt19937 gen(rd());
    std::mt19937 gen(nextRdSeed());
    return std::tan(std::acos(-1.0)*(uniDist(gen) - 0.5));
}


double PStableLSH::genUniInSigma() {
    std::uniform_real_distribution<double> uniDist(0.0, sigma);
    //std::random_device rd;
    //std::mt19937 gen(rd());
    std::mt19937 gen(nextRdSeed());
    return uniDist(gen);
}


inline HASHED_KMER PStableLSH::animoAcidHash(const char& aa) {
    return aa - 'A';
}


void PStableLSH::scanPars() {
    a = std::unordered_map <HASHED_KMER, std::vector<double> > ();
    a.reserve(2*gke.theoreticalTotalNGKMer);
    a.max_load_factor(1);

    for(int i = 0; i < node.n; ++ i) {
        auto& id = node.idList[i];
        auto& s = node.seqList.data[id];
        const int& l = s.length();

        for(int p = 0; p < l; ++ p) {
            for(auto& mask: gke.otherIndices) {
                if(p + mask.back() < l) {
                    HASHED_KMER gkmer = animoAcidHash(s[p]);
                    for(auto& j: mask) {
                        gkmer <<= 5;
                        gkmer ^= animoAcidHash(s[p+j]);
                    }
                    if(a.count(gkmer) == 0) {
                        a[gkmer] = std::vector <double> (Q);
                        for(int k = 0; k < Q; ++ k) {
                            a[gkmer][k] = genCauchy();
                        }
                    }
                }
            }
        }
    }

    b = std::vector <double> (Q);
    for(int k = 0; k < Q; ++ k) {
        b[k] = genUniInSigma();
        //std::cerr << "~~~ test seed b[" << k << "]=" << b[k] << std::endl;
    }
}


void PStableLSH::updatePars() {
    for(auto& a_gkmer: a) {
        for(int k = 0; k < Q; ++ k) {
            a_gkmer.second[k] = genCauchy();
        }
    }
    for(int k = 0; k < Q; ++ k) {
        b[k] = genUniInSigma();
        //std::cerr << "~~~ test seed b[" << k << "]=" << b[k] << std::endl;
    }
}


double PStableLSH::editDistanceCalculate(const std::string& str1, const std::string& str2) {
    int len1 = str1.size(), len2 = str2.size();
    if(! len1 || ! len2) {
        return 0.0;
    }

    std::vector< std::vector <int>> dp(len1+1, std::vector <int>(len2+1, 0));
    for(int i = 0; i <= len1; ++i) {
        dp[i][0] = i;
    }
    for(int j = 0; j <= len2; ++j) {
        dp[0][j] = j;
    }

    for(int i = 1; i <= len1; ++i) {
        for(int j = 1; j <= len2; ++j) {
            if(str1[i-1] == str2[j-1]) {
                dp[i][j] = dp[i-1][j-1];
            } else {
                dp[i][j] = std::min(dp[i-1][j-1], std::min(dp[i-1][j], dp[i][j-1])) + 1;
            }
        }
    }

    return 1 - (double)dp[len1][len2] / std::max(len1, len2);
}


struct PairComparator {
    bool operator()(const std::pair<int, int>& a, const std::pair<int, int>& b) const {
        if (a.first < b.first) return true;
        if (a.first > b.first) return false;
        return a.second < b.second;
    }
};


double PStableLSH::sample() {
    int pairNumber, similarityPairNumber = 0;

    double similarityThreshold = sim;

    std::random_device rd;
    // std::mt19937 generator(rd());
    std::mt19937 generator(nextRdSeed());
    std::uniform_int_distribution<int> distribution(0, node.n - 1);

    std::set< std::pair <int, int>, PairComparator> selected_pairs;
    if(node.n > 10000) {
        pairNumber = 10000;

        while ((int)selected_pairs.size() < pairNumber) {
            int random_num1 = distribution(generator), random_num2 = distribution(generator);
            while(random_num1 == random_num2) {
                random_num2 = distribution(generator);
            }
            selected_pairs.insert(std::make_pair(std::min(random_num1, random_num2), 
                std::max(random_num1, random_num2)));
        }
    } else {
        pairNumber = node.n;

        std::vector<std::pair<int, int>> all_pairs;
        for (int i = 0; i < node.n; ++i) {
            for (int j = i + 1; j < node.n; ++j) {
                all_pairs.push_back(std::make_pair(i, j));
            }
        }
        std::shuffle(all_pairs.begin(), all_pairs.end(), generator);
        for (int i = 0; i < pairNumber; ++i) {
            selected_pairs.insert(all_pairs[i]);
        }
    }

    for (const auto& pair : selected_pairs) {
        if(editDistanceCalculate(node.seqList.data[pair.first], node.seqList.data[pair.second]) 
            >= similarityThreshold) {
                similarityPairNumber ++;
        }
    }
    similarityPairEstimation = similarityPairNumber * 1.0 / pairNumber;
    std::cout << "--- Now similarityPairEstimation is " << similarityPairEstimation << std::endl;
    return similarityPairEstimation;
}


void PStableLSH::work() {

    for(int iter = 1; iter <= T; ++ iter) {
        //std::cout << "--- Now processing iteration " << iter << std::endl;

//auto st0 = Timer::set_start();
        std::vector <Fingerprint> table = std::vector <Fingerprint> (node.n);
        for(int i = 0; i < node.n; ++ i) {
            table[i] = Fingerprint(Q);
        }

        updatePars();
//Timer::time_profile("-- LSH work -- update pars ", st0);


//auto st1 = Timer::set_start();
        #pragma omp parallel for num_threads(Config::n_threads)
        for(int i = 0; i < node.n; ++ i) {
            std::vector <double> h(Q);
            for(int k = 0; k < Q; ++ k) {
                h[k] = 0.0;
            }

            auto& id = node.idList[i];
            auto& s = node.seqList.data[id];
            const int& l = s.length();

            for(int p = 0; p < l; ++ p) {
                for(auto& mask: gke.otherIndices) {
                    if(p + mask.back() < l) {
                        HASHED_KMER gkmer = animoAcidHash(s[p]);
                        for (auto& j: mask) {
                            gkmer <<= 5;
                            gkmer ^= animoAcidHash(s[p+j]);
                        }
                        for(int k = 0; k < Q; ++ k) {
                            h[k] += a[gkmer][k];
                        }
                    }
                }
            }

            for(int k = 0; k < Q; ++ k) {
                //h[k] /= gke.nGKmer[i] + gke.minNGKmer;
                //h[k] /= 2 * gke.nGKmer[i];
                table[i].key[k] = std::floor((h[k] + b[k]) / sigma);
            }
        }
//Timer::time_profile("-- LSH work -- hashing ", st1);


       // for(int i = 0; i < node.n; ++ i) {
       //     int id = node.idList[i];
       //     std::cout << id << ": " << node.seqList.data[id] << std::endl;
       //     for(int k = 0; k < Q; k ++) {
       //        std::cout << table[i].key[k] << ", ";
       //     }
       //     std::cout << std::endl;
       // }
        

//auto st2 = Timer::set_start();
        //Merge dsu if they shared the same fingerprint
        std::unordered_map <Fingerprint, int, FingerprintHasher> bucket;
        for(int i = 0; i < node.n; i ++) {
            if(bucket.count(table[i]) == 0) {
                bucket[table[i]] = dsu.find(i);
            } else {
                dsu.merge(bucket[table[i]], i);
            }
        }
//Timer::time_profile("-- LSH work -- merge dsu ", st2);

        //std::unordered_map <int, std::vector<int> > glanceDSU;
        //for(int i = 0; i < node.n; ++ i) {
        //    if(glanceDSU.count(dsu.find(i)) == 0) {
        //        glanceDSU[dsu.find(i)] = std::vector <int> ();
        //    }
        //    glanceDSU[dsu.find(i)].emplace_back(node.idList[i]);
        //}


       // std::cout << "~~~ Glance DSU" << std::endl;
       // for(auto v: glanceDSU) {
       //     std::cout << v.first << ": ";
       //     for(auto x: v.second) {
       //         std::cout << x << ", ";
       //     }
       //     std::cout << std::endl;
       // }
        
    }


//auto st3 = Timer::set_start();
    //Scan the dsu, and parse it to diffent sequence idlist
    subIdList = std::unordered_map <int, std::vector<int> > ();
    for(int i = 0; i < node.n; ++ i) {
        if(subIdList.count(dsu.find(i)) == 0) {
            subIdList[dsu.find(i)] = std::vector <int> ();
        }
        subIdList[dsu.find(i)].emplace_back(node.idList[i]);
    }

    if(subIdList.size() > 1) {
        for(auto x: subIdList) {
            for(auto y: x.second) {
                std::cout << y << " ";
            } 
            std::cout << std::endl;
        }
    }
//Timer::time_profile("-- LSH work -- scan dsu", st3);
}

