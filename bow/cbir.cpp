#include "opencv2/opencv.hpp"
#include "opencv2/core/ocl.hpp"
#include <opencv2/core/utils/filesystem.hpp>
#include "opencv2/xfeatures2d.hpp"

#include "BowKMajorityTrainer.h"
#include "profile.h"

#include <cstdio>
#include <vector>
#include <iostream>

using namespace std;
using namespace cv;


static void trainBinary(Ptr<DescriptorMatcher> &matcher, Ptr<BOWTrainer> &bow_train, int nclusters) {
    matcher = makePtr<BFMatcher>(NORM_HAMMING);
    bow_train = makePtr<BOWKmajorityTrainer>(nclusters, 10);
}

static void trainFloat(Ptr<DescriptorMatcher> &matcher, Ptr<BOWTrainer> &bow_train, int nclusters) {
    matcher = makePtr<FlannBasedMatcher>();
    bow_train = makePtr<BOWKMeansTrainer>(nclusters);
}

static void trainFeatures(const String &fname, Ptr<Feature2D> &det, Ptr<Feature2D> &ext, Ptr<DescriptorMatcher> &matcher, Ptr<BOWTrainer> &bow_train, int nclusters) {
   if (fname=="surf")    { det = ext = xfeatures2d::SURF::create(); trainFloat(matcher, bow_train, nclusters); }
   if (fname=="sift")    { det = ext = xfeatures2d::SIFT::create(); trainFloat(matcher, bow_train, nclusters); }
   if (fname=="vlad")    { det = ext = xfeatures2d::SURF::create(100,4,3,false,true); trainFloat(matcher, bow_train, nclusters); }
   if (fname=="vladsift"){ det = ext = xfeatures2d::SIFT::create(); trainFloat(matcher, bow_train, nclusters); }
   if (fname=="latch")   { det = ORB::create(); ext = xfeatures2d::LATCH::create(); trainBinary(matcher, bow_train, nclusters); }
   if (fname=="lucid")   { det = ORB::create(); ext = xfeatures2d::LUCID::create(3); trainBinary(matcher, bow_train, nclusters); }
   if (fname=="vgg")     { det = xfeatures2d::SURF::create(); ext = xfeatures2d::VGG::create(); trainFloat(matcher, bow_train, nclusters); }
   if (fname=="boost")   { det = xfeatures2d::SURF::create(); ext = xfeatures2d::BoostDesc::create(); trainBinary(matcher, bow_train, nclusters); }
   if (fname=="orb")     { det = ext = ORB::create(); trainBinary(matcher, bow_train, nclusters); }
   if (fname=="brisk")   { det = ORB::create(); ext = BRISK::create(); trainBinary(matcher, bow_train, nclusters); }
   if (fname=="brief")   { det = ORB::create(); ext = xfeatures2d::BriefDescriptorExtractor::create(); trainBinary(matcher, bow_train, nclusters); }
}

static Mat rootsift(Mat &feat) {
    PROFILE
    {
        PROFILEX("root:norm")
        norm(feat,feat,NORM_L1);
    }
    {
        PROFILEX("root:sqrt")
        sqrt(feat,feat);
    }
    { // for SURF (has neg numbers)
        PROFILEX("root:patch")
        patchNaNs(feat, 0);
    }
    return feat;
}

static Mat getImg(String fn, String fname) {
    Mat img = imread(fn, fname!="lucid"?0:1);
    if (! img.empty()) {
        if (img.rows>512) {
            resize(img,img,Size(), .25, .25);
        }
    }
    return img;
}

static Mat sign(const Mat &m) {
    PROFILE
    Mat s(m.size(),m.type(),1.0f);
    s.setTo(-1, m<0);
    return s;
}

static Mat vlad_feature(Ptr<Feature2D> f2d,  Ptr<DescriptorMatcher> matcher, const Mat &vocab, const Mat &img) {
    PROFILE
    std::vector<KeyPoint> kp;
    Mat feat, desc;
    {
        PROFILEX("vlad:detect")
        f2d->detectAndCompute(img, Mat(), kp, desc);
    }
    if (desc.rows>0) {
        PROFILEX("vlad:compute")
        rootsift(desc);

        vector<DMatch> matches;
        matcher->match(desc, vocab, matches);
        feat = Mat(vocab.size(), CV_32F, 0.0f);
        {
            PROFILEX("vlad:vlad")
            for (size_t j=0; j<matches.size(); j++) {
                Mat dr = desc.row(matches[j].queryIdx);
                Mat vr = vocab.row(matches[j].trainIdx);
                Mat re = vr - dr;
                normalize(re,re); // innorm
                feat.row(matches[j].trainIdx) += re / vocab.rows;
            }
        }
        { PROFILEX("vlad:post")
            // power normalization
            Mat f2;
            sqrt(abs(feat), f2);
            feat = sign(feat).mul(f2);
            // L2 normalization
            normalize(feat,feat);
        }
        return feat.reshape(1,1);
    }
    return Mat();
}

static Mat bow_feature(Ptr<Feature2D> f2d, Ptr<BOWImgDescriptorExtractor> extract, const Mat &img) {
    PROFILE
    std::vector<KeyPoint> kp;
    Mat feat;
    {
        PROFILEX("bow:detect")
        f2d->detect(img, kp);
    }
    if (kp.size()) {
        PROFILEX("bow:compute")
        extract->compute(img,kp,feat);
    }
    return feat;
}

static cv::Ptr<cv::flann::Index> train_index(const Mat &trainData) {
    if (trainData.type() == CV_8U) {
        return makePtr<cv::flann::Index>(trainData, cv::flann::LinearIndexParams(), cvflann::FLANN_DIST_HAMMING);
    }
    return makePtr<cv::flann::Index>(trainData, cv::flann::LinearIndexParams(), cvflann::FLANN_DIST_L2);
}

string category(string s, int off) {
    string s2 = s.substr(off);
    int se = s2.find_last_of('\\');
    if (se < 0)
        se = s2.find_last_of('/');
    return s2.substr(0,se);
}

int main( int argc, char ** argv ) {
    const String datapath = "c:/data/caltech/101_ObjectCategories";
    //const char *datapath = "c:/data/faces/att";
    String fname  = argc>1 ? argv[1] : "vlad";
    int nclusters = argc>2 ? atoi(argv[2]) : 64;
    int ncimages  = argc>3 ? atoi(argv[3]) : 30;
    //theRNG().state = getTickCount();
    ocl::setUseOpenCL(false);

    Ptr<Feature2D> det,ext;
    Ptr<DescriptorMatcher> matcher;
    Ptr<BOWTrainer> bow_train;
    trainFeatures(fname, det, ext, matcher, bow_train, nclusters);

    Ptr<BOWImgDescriptorExtractor> bow_extract = makePtr<BOWImgDescriptorExtractor>(ext,matcher);

    vector<String> fn;
    glob(datapath, fn, true);
    cout << fn.size() << " filenames." << endl;
    random_shuffle(fn.begin(), fn.end());

    // 1. build dictionary
    Mat vocab;
    FileStorage fs(fname+".voc.yml", 0);
    if (!fs.isOpened()) {
        int fsize=0;
        for (int i=0; i<ncimages; i++) {
            PROFILEX("add:cluster")
            int id = theRNG().uniform(0,fn.size());
            Mat img = getImg(fn[id], fname);
            if (img.empty()) continue;

            Mat feat;
            std::vector<KeyPoint> kp;
            if (det == ext) {
                det->detectAndCompute(img, Mat(), kp, feat);
            } else {
                det->detect(img, kp);
                ext->compute(img, kp, feat);
            }
            if (feat.rows>0) {
                if (fname=="vlad"||fname=="vladsift") {
                    rootsift(feat);
                }
                bow_train->add(feat);
            }

            fsize += feat.rows;
            cout << i << "\r";
            if (i%200==199)
                cout << "cluster " << fname + " " << i << " " << id << " " << feat.size() << " " << fsize << endl;
        }

        cout << "clustering " << bow_train->descriptorsCount() << " features." << endl;
        {
            PROFILEX("bow:cluster")
            vocab = bow_train->cluster();
        }

        FileStorage fs2(fname+".voc.yml", 1+FileStorage::BASE64);
        fs2 << "voc" << vocab;
        fs2.release();
    } else {
        fs["voc"] >> vocab;
        fs.release();
    }
    cerr << "bow vocab: " << vocab.size() << endl;
    bow_extract->setVocabulary(vocab);

    int fsize=0;
    Mat trainData, indices;

    //vector<String> vec_cls;
    //utils::fs::glob(datapath, "",vec_cls,false,true);
    // 2. build train data for index
    FileStorage fs1(fname+".dat.yml", 0);
    if (!fs1.isOpened()) {
        for (int i=ncimages; i<fn.size(); i++) {
            PROFILEX("train data")

            int id = i; //theRNG().uniform(0,fn.size());
            String cat = category(fn[id],datapath.size()+1);
            if (cat == "BACKGROUND_Google") continue;

            Mat img = getImg(fn[id],fname);
            if (img.empty()) continue;

            Mat feat = (fname =="vlad"||fname=="vladsift") ? vlad_feature(det, matcher, vocab, img) : bow_feature(det, bow_extract, img);
            if (feat.rows) {
                trainData.push_back(feat);
                indices.push_back(id);
            }
            fsize += feat.rows;
            cout << i << "\r";
            if (i%100==99)
                cout << "train " << fname << " " << id << " " << feat.size() << " " << fsize << endl;
        }
        FileStorage fs2(fname+".dat.yml", 1+FileStorage::BASE64);
        fs2 << "dat" << trainData;
        fs2 << "ind" << indices;
        fs2.release();
    } else {
        fs1["dat"] >> trainData;
        fs1["ind"] >> indices;
        fs1.release();
    }
    cout << "train " << fname << " " << trainData.size() << " " << trainData.type() << endl;

    // 3. build the index
    Ptr<cv::flann::Index> index;
    {
        PROFILEX("flann")
        index = train_index(trainData);
        cout << "index " << trainData.size() << " " << trainData.type() << endl;
    }

    // 4. run tests
    int K=5;
    float correct=0; int ntests=0;
    cv::flann::SearchParams params;
    for (int i=0; i<ncimages; i++) {
        int id = i;
        String cat = category(fn[id], datapath.size() + 1) ;
        if (cat == "BACKGROUND_Google") continue;
        if (cat == "faces_easy") cat = "faces";

        Mat org = getImg(fn[id],fname);
        if (org.empty()) continue;

        Mat feat = (fname =="vlad"||fname=="vladsift") ? vlad_feature(det, matcher, vocab, org) : bow_feature(det, bow_extract, org);
        if (feat.empty()) continue;
        cv::Mat dists;
        cv::Mat found;
        index->knnSearch(feat, found, dists, K, params);
        cout << "test " << cat << " " << found << endl;

        Mat res;
        resize(org,res,Size(240,160));
        for (int j=1; j<K; j++) {
            int f = found.at<int>(0,j);
            int id = indices.at<int>(f);
            Mat img = imread(fn[id],fname!="lucid"?0:1);
            if (img.empty()) continue;
            String cat2 = category(fn[id], datapath.size() + 1) ;
            if (cat2=="faces_easy") cat2="faces";
            bool ok = (cat == cat2);
            cout << "   " << ok << " " << cat2 << endl;
            correct += ok;
            ntests ++;
            resize(img,img,Size(240,160));
            hconcat(res,img,res);
        }
        imshow("I",res);
        waitKey(3000);
    }

    float acc = correct / ntests;
    cout << "final " << correct << " / " << ntests << " : " << acc << endl;

    return 0;
}