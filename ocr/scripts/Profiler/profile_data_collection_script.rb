
# Intermittant profiler errors? Doesn't seem to affect execution of program though.
def fibonacci(size, threads=4)
  `cd ~/xstack/apps/fibonacci/ocr && 
   #{clear_old_profile_data} && 
   make -f Makefile.x86-pthread-x86 clean &&
   #{ocr_config(threads)}
   make -f Makefile.x86-pthread-x86 run #{size} && 
   #{analyze_profile('fibonacci', size, size, threads)}`
   return nil
end



def sar(size, threads=4)
  case size
  when 1
    final_size = 'tiny'
  when 2
    final_size = 'small'
  else
    throw Exception.new("only 1 and 2 are valid")
  end
  `cd ~/xstack/apps/sar/ocr/#{final_size} && 
   #{clear_old_profile_data} && 
   make -f Makefile.x86-pthread-x86 clean && 
   #{ocr_config(threads)} 
   make -f Makefile.x86-pthread-x86 run && 
   #{analyze_profile('sar', size, final_size, threads)}`
   return nil
end



def cholesky(size, threads=4)
  case size
  when 1
    final_size = 10
  when 2
    final_size = 50
  when 3
    final_size = 100
  when 4
    final_size = 200
  when 5
    final_size = 300
  when 6
    final_size = 400
  when 7
    final_size = 500
  end
  `cd ~/xstack/apps/cholesky/ocr && 
   #{clear_old_profile_data} && 
   make -f Makefile.x86-pthread-x86 clean && 
   #{ocr_config(threads)} 
   make -f Makefile.x86-pthread-x86 run WORKLOAD_ARGS="--ds #{final_size} --ts 10 --fi ~/xstack/apps/cholesky/datasets/m_#{final_size}.in" &&
   #{analyze_profile('cholesky', size, final_size, threads)}`
   return nil
end



def analyze_profile(app, size, final_size, threads)
  "cd install/x86-pthread-x86 && 
  ~/xstack/ocr/scripts/Profiler/analyzeProfile.py -t '*' > ~/profile_output/profile_#{app}_#{size}_#{final_size}_#{threads}.out && 
  cd ../.."
end



def clear_old_profile_data
  'rm install/x86-pthread-x86/profile*'
end



def ocr_config(threads)
  "OCR_CONFIG=/home/mwoffendin/xstack/ocr/scripts/Configs/#{threads}_threads "
end



def setup
  generate_threads_configs
end



def generate_threads_configs
  cmd = "cd ~/xstack/ocr/scripts/Configs && "
  [1,2,4,8,16].each do |threadcount|
    cmd += "./config-generator.py --threads=#{threadcount} --output=./#{threadcount}_threads && " 
  end
  cmd += "true"
  `#{cmd}`
end



def run_all
  (1..2).each do |size|
    sar(size)
  end
  
  (1..4).each do |size|
    fibonacci(size * 2)
  end
  
  (1..4).each do |size|
    cholesky(size)
  end
  
  [1, 2, 8, 16].each do |thread_count|
    sar(2, 16)
    fibonacci(6, thread_count)
    cholesky(3, thread_count)
  end
end
