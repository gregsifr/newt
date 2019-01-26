//*******************************************************************
//    program : normal.cpp
//    author  : Uwe Wystup, http://www.mathfinance.de
//
//    created : January 2000
//
//    usage   : auxiliary functions around the normal distribution
//*******************************************************************

// normal.h

#ifndef __NORMAL_H__
#define __NORMAL_H__

// cumulative distribution function for a bivariate normal distribution, 
// see John Hull: Options, Futures and Other Derivatives
double ND2(double a, double b, double rho);
// standard normal density function
double ndf(double t); 
// standard normal cumulative distribution function
double nc(double x); 
double min(double, double);
double max(double, double);


#endif __NORMAL_H__
