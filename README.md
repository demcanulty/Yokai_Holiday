# Yokai_Holiday
# VCV Rack Dev


## TAPS


### Release Notes

2.0.0.7 - Mar 2023

> - First release

> Known bugs:
> - high frequency aliasing when location knobs/mod inputs are changed

> Feature Requests:
> - clear buffer input


![alt text](https://raw.githubusercontent.com/demcanulty/Yokai_Holiday/main/res/Readme_illustrations/TAPS_documentation.png)


## Usage Notes


###  Audible clicks when using the clock input
If the clock input is unsteady, the read/write pointers could encounter old sound data, which can cause a click.  To avoid this, once you are satisfied with your delay time, you can disconnect the clock input.  As long as you don't touch the Length knob, your delay time should now remain rock solid. 

 
## Suggestions
This one from forum user TanaBarbier — using Taps as a CV mangler for ADSR signals and LFOs:


![alt text](https://raw.githubusercontent.com/demcanulty/Yokai_Holiday/main/res/Readme_illustrations/TanaBarbier_suggestion.png)




## Bugs still to be fixed

High Frequency aliasing when changing the Distance parameters:

![alt text](https://raw.githubusercontent.com/demcanulty/Yokai_Holiday/main/res/Readme_illustrations/aliasing_bug.png)