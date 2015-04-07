/*=========================================================================
*
*  Copyright Insight Software Consortium
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*         http://www.apache.org/licenses/LICENSE-2.0.txt
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
*
*=========================================================================*/
#include <SimpleITKTestHarness.h>
#include <SimpleITK.h>

#include <stdint.h>
#include <memory>

namespace
{


class IterationUpdate
  : public itk::simple::Command
{
public:
  IterationUpdate( const itk::simple::ImageRegistrationMethod &m)
    : m_Method(m)
    {}

  std::vector<double> scales;
  std::string toString;

  virtual void Execute( )
    {
      // use sitk's output operator for std::vector etc..
      using itk::simple::operator<<;

      // stash the stream state
      std::ios  state(NULL);
      state.copyfmt(std::cout);

      if ( m_Method.GetOptimizerIteration() == 0 )
        {
        std::cout << "\tLevel: " << std::setw(3) << m_Method.GetCurrentLevel() << std::endl;
        std::cout << "\tScales: " << m_Method.GetOptimizerScales() << std::endl;
        this->scales = m_Method.GetOptimizerScales();
        this->toString = m_Method.ToString();
        }

      std::cout << std::fixed << std::setfill(' ') << std::setprecision( 5 );
      std::cout << std::setw(3) << m_Method.GetOptimizerIteration();
      std::cout << " = " << std::setw(10) << m_Method.GetMetricValue();
      std::cout << " : " << m_Method.GetOptimizerPosition() << std::endl;

      std::cout.copyfmt(state);
    }

private:
  const itk::simple::ImageRegistrationMethod &m_Method;

};

}

//
//
//

TEST(Registration,ImageRegistrationMethod_Basic) {
  // This test is to perform some basic coverage of methods.

  namespace sitk = itk::simple;

  sitk::ImageRegistrationMethod registration;
  std::cout << registration.ToString();
}


//
// Fixture based tests
//

namespace sitk = itk::simple;

class sitkRegistrationMethodTest
  : public ::testing::Test
{
public:

  sitk::Image MakeGaussianBlob( const std::vector<double> &pt,
                               const std::vector<unsigned int> &size)
    {
      sitk::GaussianImageSource source;
      source.SetMean(pt);
      source.SetScale(1.0);

      std::vector<double> sigma;
      for(unsigned int i = 0; i < size.size(); ++i)
        {
        sigma.push_back(size[i]/10.0);
        }
      source.SetSigma(sigma);
      source.SetSize(size);
      source.SetOutputPixelType(sitk::sitkFloat32);


      return source.Execute();
    }

  sitk::Image MakeDualGaussianBlobs(const std::vector<double> &pt0,
                                    const std::vector<double> &pt1,
                                    const std::vector<unsigned int> &size)
    {
       sitk::GaussianImageSource source1;
      source1.SetMean(pt0);
      source1.SetScale(1.0);

      std::vector<double> sigma;
      for(unsigned int i = 0; i < size.size(); ++i)
        {
        sigma.push_back(size[i]/10.0);
        }
      source1.SetSigma(sigma);
      source1.SetSize(size);
      source1.SetOutputPixelType(sitk::sitkFloat32);

      sitk::GaussianImageSource source2;
      source2.SetMean(pt1);
      source2.SetScale(-1.0);
      source2.SetSigma(sigma);
      source2.SetSize(size);
      source2.SetOutputPixelType(sitk::sitkFloat32);

      return sitk::Add(source1.Execute(), source2.Execute());
    }

protected:
  virtual void SetUp()
  {
    fixedBlobs = MakeDualGaussianBlobs(v2(64,64), v2(192,192), std::vector<unsigned int>(2,256));
    movingBlobs = MakeDualGaussianBlobs(v2(54,74), v2(192,192), std::vector<unsigned int>(2,256));
  }


  // virtual void TearDown() {}

  sitk::Image fixedBlobs;
  sitk::Image movingBlobs;

};


TEST_F(sitkRegistrationMethodTest, Metric_Evaluate)
{
  sitk::Image fixed = fixedBlobs;
  sitk::Image moving = fixedBlobs;


  sitk::ImageRegistrationMethod R;
  R.SetInitialTransform(sitk::Transform(fixed.GetDimension(),sitk::sitkIdentity));

  EXPECT_NEAR(-1.5299437083119216, R.MetricEvaluate(fixed,moving), 1e-10 );

  R.SetMetricAsCorrelation();
  EXPECT_NEAR(-1.0, R.MetricEvaluate(fixed,moving), 1e-10 );

  // tolerance adjusted for i386, why is it so much more?
  R.SetMetricAsJointHistogramMutualInformation( 20, 1.5);
  EXPECT_NEAR(-0.52624100016564002, R.MetricEvaluate(fixed,moving), 2e-6 );

  R.SetMetricAsMeanSquares();
  EXPECT_NEAR(0.0, R.MetricEvaluate(fixed,moving), 1e-10 );

  R.SetMetricAsMattesMutualInformation();
  EXPECT_NEAR(-1.5299437083119216, R.MetricEvaluate(fixed,moving), 1e-10 );

  R.SetMetricAsMeanSquares();

  // test that the transforms are used

  R.SetInitialTransform(sitk::TranslationTransform(fixed.GetDimension(),v2(5,-7)));
  EXPECT_NEAR(0.0036468516797954148, R.MetricEvaluate(fixed,moving), 1e-10 );

  R.SetMovingInitialTransform(sitk::TranslationTransform(fixed.GetDimension(),v2(-5,7)));
  EXPECT_NEAR(0.0, R.MetricEvaluate(fixed,moving), 1e-10 );

  R.SetFixedInitialTransform(sitk::TranslationTransform(fixed.GetDimension(),v2(-5,7)));
  EXPECT_NEAR(0.0036468516797954148, R.MetricEvaluate(fixed,moving), 1e-10 );

  sitk::ImageRegistrationMethod R2;
  R2.SetInitialTransform(sitk::Transform(fixed.GetDimension(),sitk::sitkIdentity));
  R2.SetMetricAsMeanSquares();
  R2.SetMetricFixedMask(sitk::Greater(fixedBlobs,0));
  EXPECT_NEAR(0.0091550861657971119,R2.MetricEvaluate(fixedBlobs,movingBlobs), 1e-10);

  sitk::ImageRegistrationMethod R3;
  R3.SetInitialTransform(sitk::Transform(fixed.GetDimension(),sitk::sitkIdentity));
  R3.SetMetricAsMeanSquares();
  R3.SetMetricMovingMask(sitk::Less(movingBlobs,0));
  EXPECT_NEAR(3.34e-09 ,R3.MetricEvaluate(fixedBlobs,movingBlobs), 1e-10);
}

TEST_F(sitkRegistrationMethodTest, Transform_InPlaceOn)
{
  // This test is to check the inplace operation of the initial
  // transform
  sitk::ImageRegistrationMethod R;
  EXPECT_TRUE(R.GetInitialTransformInPlace());

  sitk::Image fixed = fixedBlobs;
  sitk::Image moving = fixedBlobs;

  double minStep=1e-4;
  unsigned int numberOfIterations=100;
  double relaxationFactor=0.5;
  double gradientMagnitudeTolerance = 1e-10;
  R.SetOptimizerAsRegularStepGradientDescent(1.0,
                                             minStep,
                                             numberOfIterations,
                                             relaxationFactor,
                                             gradientMagnitudeTolerance);

  R.SetInterpolator(sitk::sitkLinear);

  sitk::TranslationTransform tx(fixed.GetDimension());
  tx.SetOffset(v2(1.1,-2.2));
  R.SetInitialTransform(tx,false);
  EXPECT_TRUE(!R.GetInitialTransformInPlace());

  R.SetMetricAsMeanSquares();

  IterationUpdate cmd(R);
  R.AddCommand(sitk::sitkIterationEvent, cmd);

  sitk::Transform outTx = R.Execute(fixed,moving);

  EXPECT_VECTOR_DOUBLE_NEAR(v2(0.0,0.0), outTx.GetParameters(), 1e-4);
  // expect input not to be modified
  EXPECT_EQ(v2(1.1,-2.2), tx.GetParameters());

  // optimize in place this time
  R.SetInitialTransform(tx,true);
  EXPECT_TRUE(R.GetInitialTransformInPlace());
  outTx = R.Execute(fixed,moving);

  EXPECT_VECTOR_DOUBLE_NEAR(v2(0.0,0.0), outTx.GetParameters(), 1e-4);
  // expect input to have been modified
  EXPECT_VECTOR_DOUBLE_NEAR(v2(0.0,0.0), tx.GetParameters(), 1e-4);


  // set with const method, with inplace constant
  const sitk::Transform &ctx =  sitk::TranslationTransform(fixed.GetDimension(),v2(0.1,-0.2));
  R.SetInitialTransform(ctx);
  EXPECT_TRUE(R.GetInitialTransformInPlace());
  outTx = R.Execute(fixed,moving);

  EXPECT_VECTOR_DOUBLE_NEAR(v2(0.0,0.0), outTx.GetParameters(), 1e-4);
  // expect input not to be modified
  EXPECT_EQ(v2(0.1,-0.2), ctx.GetParameters());

}

TEST_F(sitkRegistrationMethodTest, Transform_Initial)
{
  // This test is to check the initial transforms
  sitk::ImageRegistrationMethod R;

  sitk::Image fixed = fixedBlobs;
  sitk::Image moving = fixedBlobs;

  double minStep=1e-4;
  unsigned int numberOfIterations=100;
  double relaxationFactor=0.5;
  double gradientMagnitudeTolerance = 1e-10;
  R.SetOptimizerAsRegularStepGradientDescent(1.0,
                                             minStep,
                                             numberOfIterations,
                                             relaxationFactor,
                                             gradientMagnitudeTolerance);

  R.SetInterpolator(sitk::sitkLinear);

  sitk::TranslationTransform tx(fixed.GetDimension());
  sitk::TranslationTransform txMoving(fixed.GetDimension());
  sitk::TranslationTransform txFixed(fixed.GetDimension());
  R.SetInitialTransform(tx,false);
  R.SetMovingInitialTransform(txMoving);
  R.SetFixedInitialTransform(txFixed);

  R.SetMetricAsMeanSquares();

  IterationUpdate cmd(R);
  R.AddCommand(sitk::sitkIterationEvent, cmd);

  sitk::Transform outTx = R.Execute(fixed,moving);

  EXPECT_VECTOR_DOUBLE_NEAR(v2(0.0,0.0), outTx.GetParameters(), 1e-4);

  txMoving.SetOffset(v2(0.0,3.0));
  R.SetInitialTransform(tx,false);
  R.SetMovingInitialTransform(txMoving);
  R.SetFixedInitialTransform(txFixed);

  outTx = R.Execute(fixed,moving);

  EXPECT_VECTOR_DOUBLE_NEAR(v2(0.0,-3.0), outTx.GetParameters(), 1e-4);

  txMoving.SetOffset(v2(0.0,3.0));
  txFixed.SetOffset(v2(0.0,2.0));
  R.SetInitialTransform(tx,false);
  R.SetMovingInitialTransform(txMoving);
  R.SetFixedInitialTransform(txFixed);

  outTx = R.Execute(fixed,moving);

  EXPECT_VECTOR_DOUBLE_NEAR(v2(0.0,-1.0), outTx.GetParameters(), 1e-4);

  EXPECT_EQ(R.GetMovingInitialTransform().GetParameters(), v2(0.0,3.0));
  EXPECT_EQ(R.GetFixedInitialTransform().GetParameters(), v2(0.0,2.0));

  // test some expected exception cases

  R.SetInitialTransform(tx,false);
  R.SetMovingInitialTransform(sitk::TranslationTransform(3));
  R.SetFixedInitialTransform(txFixed);

  EXPECT_THROW(R.Execute(fixed,moving), sitk::GenericException);

  R.SetInitialTransform(tx,false);
  R.SetMovingInitialTransform(txMoving);
  R.SetFixedInitialTransform(sitk::TranslationTransform(3));

  EXPECT_THROW(R.Execute(fixed,moving), sitk::GenericException);

}



TEST_F(sitkRegistrationMethodTest, Mask_Test0)
{
  // This test is to check some excpetional cases for using masks
  sitk::ImageRegistrationMethod R;

  R.SetOptimizerAsGradientDescent(1.0, 100);

  R.SetInterpolator(sitk::sitkLinear);


  sitk::TranslationTransform tx(fixedBlobs.GetDimension());
  R.SetInitialTransform(tx);

  // wrong dimension should produce error
  R.SetMetricFixedMask(sitk::Image(100,100,100,sitk::sitkUInt8));

  EXPECT_THROW(R.Execute(fixedBlobs,movingBlobs), sitk::GenericException);

  R.SetMetricFixedMask(sitk::Image());
  // wrong dimension should produce error
  R.SetMetricMovingMask(sitk::Image(100,100,100,sitk::sitkUInt8));

  EXPECT_THROW(R.Execute(fixedBlobs,movingBlobs), sitk::GenericException);
}


TEST_F(sitkRegistrationMethodTest, Mask_Test1)
{
  // This test is to check that the metric masks have the correct
  // effect.

  sitk::ImageRegistrationMethod R;

  double learningRate=2.0;
  double minStep=1e-7;
  unsigned int numberOfIterations=100;
  double relaxationFactor=0.5;
  double gradientMagnitudeTolerance=1e-8;
  R.SetOptimizerAsRegularStepGradientDescent(learningRate,
                                             minStep,
                                             numberOfIterations,
                                             relaxationFactor,
                                             gradientMagnitudeTolerance );
  R.SetInterpolator(sitk::sitkLinear);


  sitk::TranslationTransform tx(fixedBlobs.GetDimension());
  R.SetInitialTransform(tx);

  R.SetMetricAsCorrelation();
  R.SetMetricFixedMask(sitk::Cast(sitk::Greater(fixedBlobs,0),sitk::sitkFloat32));

  IterationUpdate cmd(R);
  R.AddCommand(sitk::sitkIterationEvent, cmd);

  sitk::Transform outTx = R.Execute(fixedBlobs,movingBlobs);

  EXPECT_VECTOR_DOUBLE_NEAR(v2(-10,10), outTx.GetParameters(), 1e-4);
}


TEST_F(sitkRegistrationMethodTest, Mask_Test2)
{
  // This test is to check that the metric masks have the correct
  // effect.

  sitk::ImageRegistrationMethod R;

  double learningRate=1.0;
  double minStep=1e-7;
  unsigned int numberOfIterations=100;
  double relaxationFactor=0.5;
  double gradientMagnitudeTolerance=1e-8;
  R.SetOptimizerAsRegularStepGradientDescent(learningRate,
                                             minStep,
                                             numberOfIterations,
                                             relaxationFactor,
                                             gradientMagnitudeTolerance );
  R.SetInterpolator(sitk::sitkLinear);


  sitk::TranslationTransform tx(fixedBlobs.GetDimension());
  tx.SetOffset(v2(120,99));
  R.SetInitialTransform(tx);

  R.SetMetricAsCorrelation();
  R.SetMetricFixedMask(sitk::Greater(fixedBlobs,0));
  R.SetMetricMovingMask(sitk::Less(movingBlobs,0));

  IterationUpdate cmd(R);
  R.AddCommand(sitk::sitkIterationEvent, cmd);

  sitk::Transform outTx = R.Execute(fixedBlobs,movingBlobs);

  EXPECT_VECTOR_DOUBLE_NEAR(v2(128.0,128.0), outTx.GetParameters(), 1e-3);
}


TEST_F(sitkRegistrationMethodTest, OptimizerWeights_Test)
{
  // Test the usage of optimizer weights

  sitk::ImageRegistrationMethod R;

  double learningRate=1.0;
  double minStep=1e-7;
  unsigned int numberOfIterations=100;
  double relaxationFactor=0.5;
  double gradientMagnitudeTolerance=1e-8;
  R.SetOptimizerAsRegularStepGradientDescent(learningRate,
                                             minStep,
                                             numberOfIterations,
                                             relaxationFactor,
                                             gradientMagnitudeTolerance );
  R.SetInterpolator(sitk::sitkLinear);

  sitk::Image &fixedImage = fixedBlobs;
  sitk::Image &movingImage = fixedBlobs;

  sitk::TranslationTransform tx(fixedImage.GetDimension());
  tx.SetOffset(v2(12,9));
  R.SetInitialTransform(tx);

  R.SetMetricAsCorrelation();



  IterationUpdate cmd(R);
  R.AddCommand(sitk::sitkIterationEvent, cmd);

  std::cout << "check1\n";
  R.SetOptimizerWeights(v3(1.0,2.0,3.0));
  std::cout << "check2\n";
  EXPECT_ANY_THROW(R.Execute(fixedImage, movingImage));

  std::cout << "check3\n";
  R.SetOptimizerWeights(v2(1.0,1e10));
  sitk::Transform outTx = R.Execute(fixedImage, movingImage);

  EXPECT_VECTOR_DOUBLE_NEAR(v2(12.0,0.0031), outTx.GetParameters(), 1e-3);
}


TEST_F(sitkRegistrationMethodTest, Optimizer_Exhaustive)
{
  sitk::Image image = MakeGaussianBlob( v2(64, 64), std::vector<unsigned int>(2,256) );


  sitk::ImageRegistrationMethod R;
  R.SetInterpolator(sitk::sitkLinear);

  sitk::TranslationTransform tx(image.GetDimension());
  tx.SetOffset(v2(-1,-2));
  R.SetInitialTransform(tx, false);

  R.SetMetricAsMeanSquares();

  R.SetOptimizerAsExhaustive(std::vector<unsigned int>(2,5), 0.5);

  IterationUpdate cmd(R);
  R.AddCommand(sitk::sitkIterationEvent, cmd);

  sitk::Transform outTx = R.Execute(image, image);


  std::cout << "-------" << std::endl;
  std::cout << outTx.ToString() << std::endl;
  std::cout << "Optimizer stop condition: " << R.GetOptimizerStopConditionDescription() << std::endl;
  std::cout << " Iteration: " << R.GetOptimizerIteration() << std::endl;
  std::cout << " Metric value: " << R.GetMetricValue() << std::endl;

  EXPECT_VECTOR_DOUBLE_NEAR(v2(0.0,0.0), outTx.GetParameters(), 1e-3);


  // Execute in place

  tx.SetOffset(v2(-1,-2));
  R.SetInitialTransform(tx, true);

  R.SetOptimizerAsExhaustive( std::vector<unsigned int>(2,5), 0.5);

  outTx = R.Execute(image, image);


  std::cout << "-------" << std::endl;
  std::cout << outTx.ToString() << std::endl;
  std::cout << "Optimizer stop condition: " << R.GetOptimizerStopConditionDescription() << std::endl;
  std::cout << " Iteration: " << R.GetOptimizerIteration() << std::endl;
  std::cout << " Metric value: " << R.GetMetricValue() << std::endl;

  EXPECT_VECTOR_DOUBLE_NEAR(v2(0.0,0.0), outTx.GetParameters(), 1e-3);

}


TEST_F(sitkRegistrationMethodTest, Optimizer_Amoeba)
{
  sitk::Image image = MakeGaussianBlob( v2(64, 64), std::vector<unsigned int>(2,256) );


  sitk::ImageRegistrationMethod R;
  R.SetInterpolator(sitk::sitkLinear);

  sitk::TranslationTransform tx(image.GetDimension());
  tx.SetOffset(v2(-1,-2));
  R.SetInitialTransform(tx, false);

  R.SetMetricAsMeanSquares();

  R.SetOptimizerAsAmoeba(2.0);

  IterationUpdate cmd(R);
  R.AddCommand(sitk::sitkIterationEvent, cmd);

  sitk::Transform outTx = R.Execute(image, image);


  std::cout << "-------" << std::endl;
  std::cout << outTx.ToString() << std::endl;
  std::cout << "Optimizer stop condition: " << R.GetOptimizerStopConditionDescription() << std::endl;
  std::cout << " Iteration: " << R.GetOptimizerIteration() << std::endl;
  std::cout << " Metric value: " << R.GetMetricValue() << std::endl;

  EXPECT_VECTOR_DOUBLE_NEAR(v2(0.0,0.0), outTx.GetParameters(), 1e-3);


}


TEST_F(sitkRegistrationMethodTest, Optimizer_ScalesEstimator)
{
  sitk::Image fixedImage = MakeDualGaussianBlobs( v2(64, 64), v2(54, 74), std::vector<unsigned int>(2,256) );
  sitk::Image movingImage = MakeDualGaussianBlobs( v2(61.2, 73.5), v2(51.2, 83.5), std::vector<unsigned int>(2,256) );


  sitk::ImageRegistrationMethod R;
  R.SetInterpolator(sitk::sitkLinear);

  sitk::Euler2DTransform tx;
  R.SetInitialTransform(tx, false);

  R.SetMetricAsMeanSquares();

  unsigned int numberOfIterations=100;
  double convergenceMinimumValue = 1e-5;
  unsigned int convergenceWindowSize = 2;
  R.SetOptimizerAsConjugateGradientLineSearch( 1.0,
                                               numberOfIterations,
                                               convergenceMinimumValue,
                                               convergenceWindowSize );


  IterationUpdate cmd(R);
  R.AddCommand(sitk::sitkIterationEvent, cmd);

  R.SetOptimizerScalesFromIndexShift();
  sitk::Transform outTx = R.Execute(fixedImage, movingImage);

  EXPECT_VECTOR_DOUBLE_NEAR(v3(0.0,-2.8,9.5), outTx.GetParameters(), 0.6);
  EXPECT_VECTOR_DOUBLE_NEAR(v3(127025,1.0,1.0), cmd.scales, 1e-1);\
  EXPECT_TRUE( cmd.toString.find("ScalesFromIndexShift") != std::string::npos );


  R.SetOptimizerScalesFromJacobian();
  outTx = R.Execute(fixedImage, movingImage);

  EXPECT_VECTOR_DOUBLE_NEAR(v3(0.0,-2.8,9.5), outTx.GetParameters(), 0.6);
  EXPECT_VECTOR_DOUBLE_NEAR(v3(44198,1.0,1.0), cmd.scales, 1e-1);
  EXPECT_TRUE( cmd.toString.find("ScalesFromJacobian") != std::string::npos );


  R.SetOptimizerScalesFromPhysicalShift();
  outTx = R.Execute(fixedImage, movingImage);

  EXPECT_VECTOR_DOUBLE_NEAR(v3(0.0,-2.8,9.5), outTx.GetParameters(), 0.6);
  EXPECT_VECTOR_DOUBLE_NEAR(v3(119572,1.0,1.0), cmd.scales, 1e-1);
  EXPECT_TRUE( cmd.toString.find("ScalesFromPhysicalShift") != std::string::npos );

  R.SetOptimizerScales(v3(200000,1.0,1.0));
  outTx = R.Execute(fixedImage, movingImage);

  EXPECT_VECTOR_DOUBLE_NEAR(v3(0.0,-2.8,9.5), outTx.GetParameters(), 0.4);
  EXPECT_VECTOR_DOUBLE_NEAR(v3(200000,1.0,1.0), cmd.scales, 1e-10);


}
