#ifndef _ANALYZE_HPP_INCLUDED
#define _ANALYZE_HPP_INCLUDED
#include "pattern.hpp"
#include <unordered_map>
#include <memory>

struct AnalyzeOpts
{
  int max_iters;
  int max_population;
  int max_size;
  AnalyzeOpts()
    :max_iters(4000)
    ,max_population(1024)
    ,max_size(30)
  {};
};

struct AnalysysResult{
  enum Resolution{
    CYCLE_FOUND, ITERATIONS_EXCEEDED, PATTERN_TOO_BIG, PATTERN_TO_WIDE
  };
  int analyzed_generations;
  Resolution resolution;
  int period;
  Cell offset;
  int max_size;
  AnalysysResult()
    :analyzed_generations(-1)
    ,period(-1)
    ,offset(0,0)
    ,max_size(0)
  {};
};

class MargolusBinaryRule;

/**deprecated*/
void analyze(const Pattern &pattern_, const MargolusBinaryRule &rule, 
	     const AnalyzeOpts &options,
	     AnalysysResult &result);

class Analyzer: public AnalyzeOpts{
public:
  Analyzer( const MargolusBinaryRule &rule_ ): rule(rule_){};
  virtual AnalysysResult process( const Pattern &pattern);
  const MargolusBinaryRule &get_rule()const{ return rule; };
protected:
  const MargolusBinaryRule &rule;
  virtual void on_start_processing( const Pattern &pattern ){};
  virtual void on_iteration( int age, const Pattern &pattern ){};
  virtual void on_result_found( const Pattern &pattern, const AnalysysResult &result){};
};

class AnalyzerCache{
private:
  typedef std::unordered_map< Pattern, AnalysysResult *> cache_t;
  cache_t cache;
  std::vector<std::unique_ptr<AnalysysResult> > results;
public:
  AnalyzerCache();
  size_t put( const AnalysysResult &result );
  AnalysysResult *get_cached( const Pattern &p );
  void put( const Pattern &key, size_t result_index );
  size_t size()const{ return cache.size(); };
};

class CachingAnalyzer: public Analyzer{
  AnalyzerCache cache;
  bool cache_frozen;
  std::vector< std::unique_ptr<Pattern> > evolution;
public:
  CachingAnalyzer( const MargolusBinaryRule &rule )
    :Analyzer(rule)
    ,cache_hits(0)
    ,cache_misses(0){};
  
  bool is_frozen()const{ return cache_frozen; };
  void freeze(){ cache_frozen=true; };
  size_t cache_size()const{ return cache.size(); };

  //override
  virtual AnalysysResult process( const Pattern &pattern);

public:
  int cache_hits, cache_misses;
protected:
  virtual void on_start_processing( const Pattern &pattern );
  virtual void on_iteration( int age, const Pattern &pattern );
  virtual void on_result_found( const Pattern &pattern, const AnalysysResult &result);
};

class TreeAnalyzer: public Analyzer{
public:
  TreeAnalyzer( const MargolusBinaryRule &rule )
    :Analyzer(rule)
  {};
  //override
  virtual AnalysysResult process( const Pattern &pattern);
};

const Transform & normalizing_rotation( const Cell &offset );

class TreePattern;
AnalysysResult analyze_with_trees( const TreePattern &pattern, const MargolusBinaryRule &rule, int max_iters, int max_population, int max_size);

/**Search for the most compact form (minimizing energy) of the pattern*/
Pattern most_compact_form( const Pattern &p, size_t period, const MargolusBinaryRule &rule );

#endif
