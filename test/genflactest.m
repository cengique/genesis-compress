
% load and plot files

data1 = readgenbin('genesis-single-chan.bin');
data2 = readgenbin('genesis-single-chan.test.bin');
data3 = readgenflac('genesis-single-chan.test.genflac');
t=1:20000;
figure; plot(t, data1, t, data2, t, data3); legend('orig', 'uncomp', 'comp');

disp('max absolute error after truncation:');
disp(max(abs(data1 - data3)))
