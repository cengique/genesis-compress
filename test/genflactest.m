
% load and plot files

data1 = readgenesis('genesis-single-chan.bin', 1);
data2 = readgenesis('genesis-single-chan.test.bin', 1);
data3 = readgenesis16bit('genesis-single-chan.test.genflac');
t=1:20000;
figure; plot(t, data1, t, data2, t, data3); legend('orig', 'uncomp', 'comp');

disp('max absolute error after truncation:');
disp(max(abs(data1 - data3)))