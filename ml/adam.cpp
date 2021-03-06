#include <iostream>
#include "opencv2/opencv.hpp" 
#include "opencv2/flann/miniflann.hpp"
using namespace std;
using namespace cv;

//
// ADAM: A METHOD FOR STOCHASTIC OPTIMIZATION
//
// paper:
// http://arxiv.org/pdf/1412.6980.pdf
//
// original code from:
// https://github.com/olanleed/MochiMochi/blob/master/src/classifier/adam.hpp
//
class ADAM 
{
private :
    const std::size_t kDim;

    std::size_t _timestep;
    Mat_<float> _w;
    Mat_<float> _m;
    Mat_<float> _v;

    double suffer_loss(const Mat& x, const int y) const
    {
        return std::max(0.0, 1.0 - y * _w.dot(x));
    }

    double calculate_margin(const Mat& x) const
    {
        return _w.dot(x);
    }

public :

    ADAM(const std::size_t dim)
        : kDim(dim)
        , _timestep(0)
        , _w(Mat_<float>::zeros(1,kDim))
        , _m(Mat_<float>::zeros(1,kDim))
        , _v(Mat_<float>::zeros(1,kDim))
    {}

    virtual ~ADAM()
    {}

    bool update(const Mat &feature, const int label) 
    {
        const float kAlpha = 0.0001f;
        const float kBeta1 = 0.9f;
        const float kBeta2 = 0.999f; //0.999f;
        const float kEpsilon = 0.00000001f;
        const float kLambda = 1.0f - kEpsilon; //0.99999999f;

        if (suffer_loss(feature, label) <= 0.0) { return false; }
        _timestep++;

        float beta1_t = kBeta1 * std::pow(kLambda, float(_timestep));
        float bias1_t = (1.0f - std::pow(kBeta1, float(_timestep)));
        float bias2_t = (1.0f - std::pow(kBeta2, float(_timestep)));

        // Update biased ﬁrst moment estimate
        Mat  gradient = -label * feature;
        _m = beta1_t * _m + (1.0f - beta1_t) * gradient;

        // Compute bias-corrected ﬁrst moment estimate
        Mat m_t = _m / bias1_t;

        //Update biased second raw moment estimate
        Mat gradientSqr; 
        multiply(gradient, gradient, gradientSqr);
        _v = kBeta2  * _v + (1.0f - kBeta2)  * gradientSqr;

        // Compute bias-corrected second raw moment estimate
        Mat v_t = _v / bias2_t;
        
        // Update parameters
        cv::sqrt(v_t, v_t);
        _w -= kAlpha * m_t / (v_t + kEpsilon);

        return true;
    }

    int predict(const Mat& feature) const
    {
        double cm = calculate_margin(feature);
        return cm > 0.0 ? 1 : -1;
    }
};



//
//
/// ----->8------------------------------------------------------------------------------
//
//

void draw(const Mat &f, Mat &m,Vec3b & col)
{
    for (int i=0; i<f.cols; i++)
    {
        float fv = 50 * f.at<float>(0,i);
        if (fv<1 || fv >= m.rows) continue;
        m.at<Vec3b>(m.rows-int(fv),i) = col; 
    }
}


int main(int argc, char* argv[])
{
    namedWindow("s",0);
    int N=600;
    int S=600;
    float NOISE = 0.2f;
    cv::Mat features(N,S,CV_32F);
    randu(features, 0.0f, 1.0f);
    
    // the 'ideal' signal:
    Mat curve(1,S,CV_32F);
    for (int j=0; j<S; j++)
    {
        curve.at<float>(0,j) = sin(j*.6) * cos(3.2/(j+1));
    }
    
    // initialize even samples to positive class
    //  (the odd ones stay random)
    for (int i=0; i<N; i+=2)
    {
        Mat noise(curve.size(), curve.type());
        randu(noise, 0.0f, NOISE);
        features.row(i) = curve + noise;
    }

    // train ADAM on 1st half:
    ADAM adam(S);
    int i=0;
    for (; i<N/2; i++)
    {
        int label = ((i%2==0) ? 1 : -1);
        adam.update(features.row(i), label);
    }

    // test ADAM on 2nd half:
    int miss = 0;
    for (; i<N; i++)
    {
        int label = ((i%2==0) ? 1 : -1);
        int pred  = adam.predict(features.row(i));
        miss += (label != pred);

        //cerr << label << " " << pred << endl;
        Mat d(150,S,CV_8UC3,Scalar::all(0));
        draw(curve, d, Vec3b(0,0,200));
        draw(features.row(i), d, Vec3b(0,200,0));
        imshow("s",d);
        waitKey(20);
    }
    cerr << N/2 << " test items, " << miss << " missed." << endl;
    return 0;
}
